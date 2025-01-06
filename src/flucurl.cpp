#include "flucurl.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include "build/vcpkg_installed/x64-windows/include/curl/curl.h"
#include "build/vcpkg_installed/x64-windows/include/curl/easy.h"
#include "build/vcpkg_installed/x64-windows/include/curl/multi.h"
#include "build/vcpkg_installed/x64-windows/include/curl/urlapi.h"

#else
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <curl/urlapi.h>

#endif

class Session;
using namespace std::chrono;
class HTTPMemoryManager {
 public:
  HTTPMemoryManager()
      : headerPool(&headerUpstream),
        bodyPool(&bodyUpstream),
        headerResource(&headerPool),
        bodyResource(&bodyPool) {}

  void *allocateHeader(size_t size) { return headerResource.allocate(size); }

  void deallocateHeader(char *ptr, size_t size) {
    headerResource.deallocate(ptr, size);
  }

  void *allocateBody(size_t size) { return bodyResource.allocate(size); }

  void deallocateBody(char *ptr, size_t size) {
    bodyResource.deallocate(ptr, size);
  }
  ~HTTPMemoryManager() { std::cout << "memory manager destructed\n"; }

 private:
  // Upstream resources to handle overflow
  std::pmr::unsynchronized_pool_resource headerUpstream;
  std::pmr::unsynchronized_pool_resource bodyUpstream;

  std::pmr::unsynchronized_pool_resource headerPool;
  std::pmr::unsynchronized_pool_resource bodyPool;
  std::pmr::polymorphic_allocator<char> headerResource;
  std::pmr::polymorphic_allocator<char> bodyResource;
};

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
struct TaskData {
  std::vector<Field> header_entries = {};
  Request request = {};
  ResponseCallback callback = {};
  DataHandler onData = {};
  ErrorHandler onError = {};
  Response response = {};
  Session *session = nullptr;
  UploadState *upload_state = nullptr;
};

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata);

template <typename T>
class ObjectPool {
  std::vector<T *> items;
  std::mutex mtx;
  uint32_t max_size;

 public:
  T *acquire_item() {
    std::unique_lock<std::mutex> lk;
    if (items.empty()) {
      return new T();
    }
    T *item = items.back();
    items.pop_back();
    return item;
  }
  void release_item(T *item) {
    std::unique_lock<std::mutex> lk;
    if (items.size() >= max_size) {
      delete item;
      return;
    }
    *item = {};
    items.push_back(item);
  }
  ObjectPool(int32_t max_size = 15) : max_size(max_size) {}
  ~ObjectPool() {
    std::cout << "destruct object pool\n";
    for (auto item : items) {
      delete item;
    }
  }
};

class Session {
  // you should lock outside
  CURL *acquire_handle() {
    if (handles.empty()) {
      CURL *curl = curl_easy_duphandle(handle_prototype);
      return curl;
    }
    CURL *curl = handles.back();
    handles.pop_back();
    return curl;
  }

  // you should lock outside
  void release_handle(CURL *curl) {
    // when there is too many idle handles
    if (handles.size() >= 15) {
      curl_easy_cleanup(curl);
      return;
    }
    handles.push_back(curl);
  }
  ObjectPool<TaskData> request_task_pool;

 public:
  ObjectPool<UploadState> upload_state_pool;
  HTTPMemoryManager memory_manager;
  std::unordered_map<CURL *, TaskData *> requests;
  std::mutex request_mtx, handle_mtx;
  CURLM *multi_handle = nullptr;
  std::vector<CURL *> handles;
  std::unique_ptr<std::thread> worker;
  bool should_exit = false;
  int running_handles = 0;
  Config config;
  CURL *handle_prototype;

  UploadState *add_request(Request request, ResponseCallback callback,
                           DataHandler onData, ErrorHandler onError) {
    std::unique_lock<std::mutex> lk{request_mtx};
    CURL *curl = acquire_handle();
    if (curl == nullptr) {
      onError("Unable to initialize curl");
      return nullptr;
    }

    auto *data = request_task_pool.acquire_item();
    data->session = this;
    data->onData = onData;
    data->onError = onError;
    data->callback = callback;
    data->request = request;
    data->response = {};
    data->response.session = this;

    // set http body
    auto *state = upload_state_pool.acquire_item();
    state->mtx = new std::mutex();
    state->session = this;
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, state);
    data->upload_state = state;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.content_length);

    // set header receive callback
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);

    // set body receive callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

    // set http method
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method);

    // set http headers
    curl_slist *list = nullptr;
    for (int i = 0; i < request.header_count; i++) {
      list = curl_slist_append(list, request.headers[i]);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    CURLcode ret = curl_easy_setopt(curl, CURLOPT_URL, request.url);
    if (ret != CURLE_OK) {
      onError("Unable to set URL");
      return nullptr;
    }

    requests[curl] = data;

    curl_multi_add_handle(multi_handle, curl);
    return state;
  }

  void remove_request(CURL *curl) {
    std::unique_lock<std::mutex> lk{request_mtx};
    auto it = requests.find(curl);
    if (it != requests.end()) {
      auto mtx = static_cast<std::mutex *>(it->second->upload_state->mtx);
      delete mtx;
      auto queue =
          static_cast<std::queue<Field> *>(it->second->upload_state->queue);
      delete queue;
      delete it->second->upload_state;
      request_task_pool.release_item(it->second);
      requests.erase(it);
      curl_multi_remove_handle(multi_handle, curl);
      release_handle(curl);
    }
  }

  Session() {}

  ~Session() {}

  void report_done(CURL *curl) {
    std::unique_lock<std::mutex> lk{request_mtx};
    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onData({nullptr, 0});
    }
  }

  void report_error(CURL *curl, const char *message) {
    std::unique_lock<std::mutex> lk{request_mtx};
    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onError(message);
    }
  }
};

auto flucurl_session_init(Config config) -> void * {
  auto *session = new Session();
  session->config = config;
  CURL *curl = curl_easy_init();
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
  session->handle_prototype = curl;

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

// You should only call this function when you ensure all requests has
// completed.
auto flucurl_session_terminate(void *p) -> void {
  auto *session = static_cast<Session *>(p);
  session->should_exit = true;
  if (session->worker->joinable()) session->worker->join();
  session->worker = nullptr;
  curl_multi_cleanup(session->multi_handle);
  for (auto handle : session->handles) {
    curl_easy_cleanup(handle);
  }
  curl_easy_cleanup(session->handle_prototype);
  delete session;
}

UploadState *flucurl_session_send_request(void *p, Request request,
                                          ResponseCallback callback,
                                          DataHandler onData,
                                          ErrorHandler onError) {
  auto *session = static_cast<Session *>(p);
  return session->add_request(request, callback, onData, onError);
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

void flucurl_free_reponse(Response response) {
  auto session = static_cast<Session *>(response.session);
  for (int i = 0; i < response.header_count; i++) {
    auto header = response.headers[i];
    session->memory_manager.deallocateHeader(header.p, header.len);
  }
  delete[] response.headers;
}
void flucurl_free_bodydata(BodyData body_data) {
  auto *session = static_cast<Session *>(body_data.session);
  session->memory_manager.deallocateBody(body_data.data, body_data.size);
}

void flucurl_unlock_upload(UploadState s) {
  auto mtx = static_cast<std::mutex *>(s.mtx);
  mtx->unlock();
}

void flucurl_lock_upload(UploadState s) {
  auto mtx = static_cast<std::mutex *>(s.mtx);
  mtx->lock();
}

void flucurl_resume_upload(UploadState s) {
  s.pause = false;
  curl_easy_pause(s.curl, CURLPAUSE_CONT);
}

size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  auto state = static_cast<UploadState *>(userdata);
  auto mtx = static_cast<std::mutex *>(state->mtx);
  auto queue = static_cast<std::queue<Field> *>(state->queue);
  auto session = static_cast<Session *>(state->session);
  std::unique_lock lk{*mtx};
  auto field = queue->front();
  if (!field.p) {
    return 0;
  }
  size_t remaining = field.len - state->cur;
  if (!remaining) {
    state->pause = true;
    return CURL_READFUNC_PAUSE;
  }
  auto dest = static_cast<char *>(ptr);
  size_t len = std::min(total_size, remaining);
  std::copy(field.p + state->cur, field.p + state->cur + len, dest);
  state->cur += len;
  if (state->cur >= field.len) {
    session->config.free_dart_memory(field.p);
    state->cur = 0;
    queue->pop();
  }
  return len;
}
void flucurl_upload_append(UploadState s, Field f) {
  auto mtx = static_cast<std::mutex *>(s.mtx);
  auto queue = static_cast<std::queue<Field> *>(s.queue);
  std::unique_lock<std::mutex> lk{*mtx};
  queue->push(f);
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *cb_data = static_cast<TaskData *>(userdata);
  size_t total_size = size * nmemb;
  auto *body_ptr = static_cast<char *>(ptr);
  auto *data = cb_data->session->memory_manager.allocateBody(total_size);
  std::copy(body_ptr, body_ptr + total_size, static_cast<char *>(data));

  BodyData body_data;
  body_data.session = cb_data->session;
  body_data.data = static_cast<char *>(data);
  body_data.size = total_size;
  cb_data->onData(body_data);
  return total_size;
}

size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *header_data = static_cast<TaskData *>(userdata);
  int total_size = size * nmemb;
  auto *header_line = static_cast<char *>(ptr);
  if (strncmp(header_line, "\r\n", 2) == 0) {
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

  void *data =
      header_data->session->memory_manager.allocateHeader(total_size - 2);
  char *header_kv = static_cast<char *>(data);

  std::copy(header_line, header_line + total_size - 2, header_kv);

  header_data->header_entries.push_back(
      {.p = header_kv, .len = total_size - 2});

  return total_size;
}