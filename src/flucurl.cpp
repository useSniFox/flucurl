#include "flucurl.h"

#include <basetsd.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "build/vcpkg_installed/x64-windows/include/curl/multi.h"

#ifdef _WIN32
#include "build/vcpkg_installed/x64-windows/include/curl/curl.h"
#include "build/vcpkg_installed/x64-windows/include/curl/easy.h"
#include "build/vcpkg_installed/x64-windows/include/curl/urlapi.h"
#else
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/urlapi.h>
#endif

using namespace std::chrono;

uint32_t rng(uint32_t min, uint32_t max) {
  static std::random_device rd;
  static auto gen =
      rd.entropy()
          ? std::mt19937(rd())
          : std::mt19937(system_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<uint32_t> dis(min, max);
  return dis(gen);
}

std::string generate_request_id(Request const &r) {
  std::stringstream builder{};
  builder << r.url << '-' << system_clock::now().time_since_epoch().count()
          << '-' << rng(0, 0xFFFFFFFF);
  return builder.str();
}

struct RequestTaskData {
  std::vector<Field> header_entries;
  Request request;
  ResponseCallback callback;
  DataHandler onData;
  ErrorHandler onError;
  Response response;
};

// 回调函数，用于处理响应数据
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  auto *body_ptr = static_cast<char *>(ptr);
  std::cout << "chunk size: " << total_size << std::endl;
  auto *data = new char[total_size];
  std::copy(body_ptr, body_ptr + total_size, data);

  auto *cb_data = static_cast<RequestTaskData *>(userdata);
  BodyData body_data;
  body_data.data = data;
  body_data.size = total_size;
  cb_data->onData(body_data);
  return total_size;
}

// 回调函数：处理 HTTP 响应头
size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *header_data = static_cast<RequestTaskData *>(userdata);

  size_t total_size = size * nmemb;
  auto *header_line = static_cast<char *>(ptr);
  if (total_size < 2 || strncmp(header_line, "\r\n", 2) == 0) {
    // header finish

    auto *header = new Field[header_data->header_entries.size()];
    std::copy(header_data->header_entries.begin(),
              header_data->header_entries.end(), header);
    header_data->response.headers = header;
    header_data->response.header_count = header_data->header_entries.size();
    header_data->callback(header_data->response);
    std::cout << "Header Finished" << std::endl;
    return total_size;
  }
  if (!header_data->response.status) {
    std::cout << header_line << std::endl;
    std::istringstream sin(header_line);
    std::string version;
    int status_code;
    sin >> version >> status_code;
    header_data->response.status = status_code;
    if (version == "HTTP/1.1") {
      header_data->response.http_version = "1.1";
    } else if (version == "HTTP/2") {
      header_data->response.http_version = "2";
    } else if (version == "HTTP/3") {
      header_data->response.http_version = "3";
    } else {
      header_data->response.http_version = "1.0";
    }
    return total_size;
  }

  auto *pos = std::find(header_line, header_line + total_size, ':');

  int key_len = pos - header_line;
  char *key = new char[key_len + 1];
  std::copy(header_line, pos, key);
  key[key_len] = '\0';

  int value_len = total_size - key_len - 4;
  char *value = new char[value_len + 1];
  std::copy(pos + 2, header_line + total_size - 2, value);
  value[value_len] = '\0';

  std::cout << key << " " << value << std::endl;
  header_data->header_entries.push_back(Field{.key = key, .value = value});
  fflush(stdout);

  return total_size;
}

class Session {
 public:
  std::unordered_map<CURL *, RequestTaskData *> requests;
  CURLM *multi_handle = nullptr;
  std::unique_ptr<std::thread> worker;
  bool should_exit = false;
  int running_handles = 0;

  void add_request(Request request, ResponseCallback callback,
                   DataHandler onData, ErrorHandler onError) {
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
      onError("Unable to initialize curl");
      return;
    }

    auto *data = new RequestTaskData();
    data->onData = onData;
    data->onError = onError;
    data->callback = callback;
    data->request = request;
    data->response = {};

    requests[curl] = data;

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_SSLENGINE, "dynamic");
    curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1l);

    CURLcode ret = curl_easy_setopt(curl, CURLOPT_URL, request.url);
    if (ret != CURLE_OK) {
      onError("Unable to set URL");
      return;
    }
  }

  void remove_request(CURL *curl) {
    auto it = requests.find(curl);
    if (it != requests.end()) {
      requests.erase(it);
      delete it->second;
      curl_multi_remove_handle(multi_handle, curl);
      curl_easy_cleanup(curl);
    }
  }

  Session() {}
  ~Session() {}

  void report_done(CURL *curl) {
    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onData({nullptr, 0});
    }
  }

  void report_error(CURL *curl, const char *message) {
    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onError(message);
    }
  }
};

auto init_session(Config const &config) -> Session * {
  auto *session = new Session();
  session->multi_handle = curl_multi_init();
  session->worker = std::make_unique<std::thread>([session]() {
    do {
      CURLMcode mc =
          curl_multi_perform(session->multi_handle, &session->running_handles);
      // Check if there are completed messages
      CURLMsg *msg;
      int msgs_left;
      while ((msg = curl_multi_info_read(session->multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
          CURL *handle = msg->easy_handle;
          if (msg->data.result != CURLE_OK) {
            session->report_error(handle, curl_easy_strerror(msg->data.result));
          } else {
            std::cout << "Request completed successfully." << std::endl;
            // Handle your data here (e.g., retrieve response)
            session->report_done(handle);
          }
          session->remove_request(handle);
        }
      }

      if (session->running_handles > 0) {
        mc = curl_multi_poll(session->multi_handle, nullptr, 0, 1000, nullptr);
        if (mc != CURLM_OK) {
          std::cerr << "curl_multi_poll error: " << curl_multi_strerror(mc)
                    << std::endl;
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    } while (!session->should_exit);
  });
  return session;
}

auto session_terminate(Session *session) -> void {
  curl_multi_cleanup(session->multi_handle);
  session->should_exit = true;
  if (session->worker->joinable()) session->worker->join();
  session->worker = nullptr;
  delete session;
}

void session_send_request(Session *session, Request request,
                          ResponseCallback callback, DataHandler onData,
                          ErrorHandler onError) {
  session->add_request(request, callback, onData, onError);
}

void global_init() {
  int ret = curl_global_init(CURL_GLOBAL_ALL);
  if (ret != CURLE_OK) {
    std::cout << "Unable to initialize curl" << std::endl;
  } else {
    std::cout << "Curl initialized" << std::endl;
  }
}

void flucurl_free_reponse(Response p) {
  for (int i = 0; i < p.header_count; i++) {
    delete[] p.headers[i].key;
    delete[] p.headers[i].value;
  }
  delete[] p.headers;
}
void flucurl_free_bodydata(const char *p) { delete[] p; }