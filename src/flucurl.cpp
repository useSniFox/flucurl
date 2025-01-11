#include "flucurl.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <curl/urlapi.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

class Session;
using namespace std::chrono;
class MemoryManager {
  std::pmr::synchronized_pool_resource pool;
  std::pmr::polymorphic_allocator<char> resource;

 public:
  MemoryManager() : resource(&pool) {}
  void *allocate(size_t size) { return resource.allocate(size); }
  void deallocate(void *p, size_t size) {
    resource.deallocate(static_cast<char *>(p), size);
  }
};

MemoryManager header_manager, body_manager;

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

void session_worker_func(Session *session);

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
  ObjectPool(int32_t max_size = 50) : max_size(max_size) {}
  ~ObjectPool() {
    for (auto item : items) {
      delete item;
    }
  }
};

ObjectPool<BodyData> body_data_pool;

class Session {
 public:
  // only call this in worker thread
  // you should lock outside
  CURL *acquire_handle() {
    if (!handles.empty()) {
      CURL *curl = handles.back();
      handles.pop_back();
      return curl;
    }
    if (total_handle < 50) {
      total_handle++;
      CURL *curl = curl_easy_duphandle(handle_prototype);
      curl_easy_setopt(curl, CURLOPT_SHARE, share_handle);
      return curl;
    }
    return nullptr;
  }

  // only call this in worker thread
  // you should lock outside
  void release_handle(CURL *curl) {
    // when there is too many idle handles
    handles.push_back(curl);
  }
  ObjectPool<TaskData> request_task_pool;
  ObjectPool<UploadState> upload_state_pool;

  // only call this in worker thread
  void perform_request(CURL *curl, TaskData *task) {
    UploadState *state = task->upload_state;
    Request request = task->request;
    curl_easy_setopt(curl, CURLOPT_READDATA, state);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.content_length);

    // set header receive callback
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, task);

    // set body receive callback
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, task);

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
      task->onError("Unable to set URL");
      return;
    }
    requests[curl] = task;

    curl_multi_add_handle(multi_handle, curl);
  }

  std::unordered_map<CURL *, TaskData *> requests;
  std::mutex task_queue_mtx{};
  CURLM *multi_handle = nullptr;
  CURLSH *share_handle = nullptr;
  std::vector<CURL *> handles;
  std::unique_ptr<std::thread> worker;
  std::queue<TaskData *> task_queue;
  int total_handle = 0;
  bool should_exit = false;
  int running_handles = 0;
  Config config;
  CURL *handle_prototype;

  UploadState *add_request(Request request, ResponseCallback callback,
                           DataHandler onData, ErrorHandler onError) {
    auto *task = request_task_pool.acquire_item();
    task->session = this;
    task->onData = onData;
    task->onError = onError;
    task->callback = callback;
    task->request = request;
    task->response = {};
    task->response.session = this;

    // set http body
    auto *state = upload_state_pool.acquire_item();
    state->queue = new std::queue<Field>();
    state->mtx = new std::mutex();
    state->session = this;
    task->upload_state = state;

    {
      std::unique_lock lk{task_queue_mtx};
      task_queue.push(task);
      curl_multi_wakeup(multi_handle);
    }
    return state;
  }

  // only called by worker thread
  void remove_request(CURL *curl) {
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

  // only called by worker thread
  void report_done(CURL *curl) {
    auto it = requests.find(curl);
    if (it != requests.end()) {
      it->second->onData(nullptr);
    }
  }

  // only called by worker thread
  void report_error(CURL *curl, const char *message) {
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

  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10L);

  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

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

  // set share handle, share data connections and dns cache
  CURLSH *share_handle = curl_share_init();
  curl_share_setopt(share_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  curl_share_setopt(share_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  session->share_handle = share_handle;

  CURLM *multi_handle = curl_multi_init();
  // enable HTTP2 multiplexing by default
  curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
  session->multi_handle = multi_handle;

  session->worker = std::make_unique<std::thread>(session_worker_func, session);
  return session;
}

void session_worker_func(Session *session) {
  do {
    {
      std::unique_lock lk{session->task_queue_mtx};
      while (!session->task_queue.empty()) {
        auto task = session->task_queue.front();
        if (CURL *curl = session->acquire_handle()) {
          session->task_queue.pop();
          session->perform_request(curl, task);
        } else {
          break;
        }
      }
    }
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
          session->report_done(handle);
        }
        session->remove_request(handle);
      }
    }
    mc = curl_multi_poll(session->multi_handle, nullptr, 0, 10, nullptr);
    if (mc != CURLM_OK) {
      std::cerr << "curl_multi_poll error: " << curl_multi_strerror(mc)
                << std::endl;
      break;
    }
  } while (!session->should_exit);
}

// You should only call this function when you ensure all requests has
// completed.
auto flucurl_session_terminate(void *p) -> void {
  auto *session = static_cast<Session *>(p);
  session->should_exit = true;
  curl_multi_wakeup(session->multi_handle);
  session->worker->join();
  session->worker = nullptr;
  curl_multi_cleanup(session->multi_handle);
  for (auto handle : session->handles) {
    curl_easy_cleanup(handle);
  }
  curl_easy_cleanup(session->handle_prototype);
  curl_share_cleanup(session->share_handle);
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
    header_manager.deallocate(header.p, header.len);
  }
  delete[] response.headers;
}
void flucurl_free_bodydata(BodyData *body_data) {
  auto *session = static_cast<Session *>(body_data->session);
  body_manager.deallocate(body_data->data, body_data->size);
  body_data_pool.release_item(body_data);
}

void flucurl_unlock_upload(UploadState s) {
  auto mtx = static_cast<std::mutex *>(s.mtx);
  mtx->unlock();
}

void flucurl_lock_upload(UploadState s) {
  auto mtx = static_cast<std::mutex *>(s.mtx);
  mtx->lock();
}

size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  auto state = static_cast<UploadState *>(userdata);
  auto mtx = static_cast<std::mutex *>(state->mtx);
  auto queue = static_cast<std::queue<Field> *>(state->queue);
  auto session = static_cast<Session *>(state->session);
  std::unique_lock lk{*mtx};
  if (queue->empty()) {
    state->pause = true;
    return CURL_READFUNC_PAUSE;
  }
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
  size_t len = (((total_size) < (remaining)) ? (total_size) : (remaining));
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
  s.pause = false;
  curl_easy_pause(s.curl, CURLPAUSE_CONT);
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *cb_data = static_cast<TaskData *>(userdata);
  if (cb_data->response.status) {
    auto *header = new Field[cb_data->header_entries.size()];
    std::copy(cb_data->header_entries.begin(), cb_data->header_entries.end(),
              header);
    cb_data->response.headers = header;
    cb_data->response.header_count = cb_data->header_entries.size();
    cb_data->callback(cb_data->response);
    cb_data->response.status = 0;
  }
  size_t total_size = size * nmemb;
  auto *body_ptr = static_cast<char *>(ptr);
  auto *data = body_manager.allocate(total_size);
  std::copy(body_ptr, body_ptr + total_size, static_cast<char *>(data));

  BodyData *body_data = body_data_pool.acquire_item();
  body_data->session = cb_data->session;
  body_data->data = static_cast<char *>(data);
  body_data->size = total_size;
  cb_data->onData(body_data);
  return total_size;
}

size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *header_data = static_cast<TaskData *>(userdata);
  int total_size = size * nmemb;
  auto *header_line = static_cast<char *>(ptr);
  if (std::strncmp(header_line, "\r\n", 2) == 0) {
    // skip empty line
    return total_size;
  }
  if (std::strncmp(header_line, "HTTP/", 5) == 0) {
    // possibly a http message header
    bool yes = false;
    int status_code;
    // std::cout << header_line << std::endl;
    std::istringstream sin(header_line);
    std::string version;
    sin >> version >> status_code;
    header_data->response.status = status_code;
    if (std::strncmp(header_line + 5, "1.1 ", 4) == 0) {
      header_data->response.http_version = HTTP1_1;
    } else if (std::strncmp(header_line + 5, "2 ", 2) == 0) {
      header_data->response.http_version = HTTP2;

    } else if (std::strncmp(header_line + 5, "3 ", 2) == 0) {
      header_data->response.http_version = HTTP3;

    } else if (std::strncmp(header_line + 5, "1.0 ", 4) == 0) {
      header_data->response.http_version = HTTP1_0;
    }
    return total_size;
  }

  void *data = header_manager.allocate(total_size - 2);
  char *header_kv = static_cast<char *>(data);

  // strip the trailing "\r\n"
  std::copy(header_line, header_line + total_size - 2, header_kv);

  header_data->header_entries.push_back(
      {.p = header_kv, .len = total_size - 2});

  return total_size;
}