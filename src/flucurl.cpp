#include "flucurl.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include "build/vcpkg_installed/x64-windows/include/curl/curl.h"
#include "build/vcpkg_installed/x64-windows/include/curl/easy.h"
#include "build/vcpkg_installed/x64-windows/include/curl/urlapi.h"
#include "build/vcpkg_installed/x64-windows/include/curl/multi.h"
#else
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/urlapi.h>
#include <curl/multi.h>
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
  std::ostringstream builder{};
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
    auto *header = new Field[header_data->header_entries.size()];
    std::copy(header_data->header_entries.begin(),
              header_data->header_entries.end(), header);
    header_data->response.headers = header;
    header_data->response.header_count = header_data->header_entries.size();
    header_data->callback(header_data->response);
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
      header_data->response.http_version = HTTP1_1;
    } else if (version == "HTTP/2") {
      header_data->response.http_version = HTTP2;
    } else if (version == "HTTP/3") {
      header_data->response.http_version = HTTP3;
    } else {
      header_data->response.http_version = HTTP1_0;
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

  header_data->header_entries.push_back({.key = key, .value = value});

  return total_size;
}

class Session {
 public:
  std::unordered_map<CURL *, RequestTaskData *> requests;
  std::mutex mtx;
  CURLM *multi_handle = nullptr;
  std::unique_ptr<std::thread> worker;
  bool should_exit = false;
  int running_handles = 0;
  Config config;

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
    std::unique_lock<std::mutex> lk{mtx};

    // set header receive callback
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);

    // set body receive callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

    // set default ssl support
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_SSLENGINE, "dynamic");
    curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1l);

    switch (config.http_version) {
      case HTTP1_0:
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
        break;
      case HTTP1_1:
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        break;
      case HTTP2:
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        break;
      case HTTP3:
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
        break;
      default:
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
        break;
    }

    // set response timeout
    if (config.timeout) {
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.timeout);
    }

    // set http proxy
    if (config.proxy) {
      curl_easy_setopt(curl, CURLOPT_PROXY, config.proxy);
    }

    // set tcp keep alive
    if (config.keep_alive) {
      curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, config.keep_alive);
    }

    // set tcp idle timeout
    if (config.idle_timeout) {
      curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, config.idle_timeout);
    }

    CURLcode ret = curl_easy_setopt(curl, CURLOPT_URL, request.url);
    if (ret != CURLE_OK) {
      onError("Unable to set URL");
      return;
    }
    requests[curl] = data;

    curl_multi_add_handle(multi_handle, curl);
  }

  void remove_request(CURL *curl) {
    std::unique_lock<std::mutex> lk{mtx};

    auto it = requests.find(curl);
    if (it != requests.end()) {
      delete it->second;
      requests.erase(it);
      curl_multi_remove_handle(multi_handle, curl);
      curl_easy_cleanup(curl);
    }
  }

  Session() {}
  ~Session() {}

  void report_done(CURL *curl) {
    std::unique_lock<std::mutex> lk{mtx};

    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onData({nullptr, 0});
    }
  }

  void report_error(CURL *curl, const char *message) {
    std::unique_lock<std::mutex> lk{mtx};
    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onError(message);
    }
  }
};

auto session_init(Config config) -> void * {
  auto *session = new Session();
  session->config = config;
  CURLM *multi_handle = curl_multi_init();
  // enable HTTP2 multiplexing by default
  curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
  session->multi_handle = multi_handle;
  session->worker = std::make_unique<std::thread>([session]() {
    do {
      CURLMcode mc =
          curl_multi_perform(session->multi_handle, &session->running_handles);
      if (mc != CURLM_OK) {
        std::cout << "Multi error: " << curl_multi_strerror(mc) << std::endl;
        std::exit(1);
      }
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

auto session_terminate(void *p) -> void {
  auto *session = static_cast<Session *>(p);
  session->should_exit = true;
  if (session->worker->joinable()) session->worker->join();
  session->worker = nullptr;
  curl_multi_cleanup(session->multi_handle);
  delete session;
}

void session_send_request(void *p, Request request, ResponseCallback callback,
                          DataHandler onData, ErrorHandler onError) {
  auto *session = static_cast<Session *>(p);
  session->add_request(request, callback, onData, onError);
}

void flucurl_global_init() {
  int ret = curl_global_init(CURL_GLOBAL_ALL);
  if (ret != CURLE_OK) {
    std::cout << "Unable to initialize curl" << std::endl;
  } else {
    std::cout << "Curl initialized" << std::endl;
  }
}

void flucurl_global_deinit() { curl_global_cleanup(); }

void flucurl_free_reponse(Response p) {
  for (int i = 0; i < p.header_count; i++) {
    delete[] p.headers[i].key;
    delete[] p.headers[i].value;
  }
  delete[] p.headers;
}
void flucurl_free_bodydata(const char *p) { delete[] p; }