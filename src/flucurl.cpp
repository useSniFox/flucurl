#include "flucurl.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "build/vcpkg_installed/x64-windows/include/curl/curl.h"
#include "build/vcpkg_installed/x64-windows/include/curl/easy.h"

using namespace std::chrono;
void init() {
  int ret = curl_global_init(CURL_GLOBAL_ALL);
  if (ret != CURLE_OK) {
    std::cout << "Unable to initialize curl" << std::endl;
  } else {
    std::cout << "Curl initialized" << std::endl;
  }
}

struct HeaderCallbackData {
  std::vector<Field> entries;
  // bool is_first;
  const char *first_line = nullptr;
  Response *response;
  ResponseCallback callback;
};

struct BodyCallbackData {
  DataHandler callback;
};

// 回调函数，用于处理响应数据
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  auto *body_ptr = static_cast<char *>(ptr);
  std::cout << "body: " << total_size << std::endl;
  auto *data = new char[total_size];
  std::copy(body_ptr, body_ptr + total_size, data);

  auto *cb_data = static_cast<BodyCallbackData *>(userdata);
  BodyData body_data;
  body_data.data = data;
  body_data.size = total_size;
  cb_data->callback(body_data);
  return total_size;
}

// 回调函数：处理 HTTP 响应头
size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *header_data = static_cast<HeaderCallbackData *>(userdata);

  size_t total_size = size * nmemb;
  auto *header_line = reinterpret_cast<char *>(ptr);
  if (total_size < 2 || strncmp(header_line, "\r\n", 2) == 0) {
    auto *header = new Field[header_data->entries.size()];

    std::copy(header_data->entries.begin(), header_data->entries.end(), header);
    header_data->response->header = header;
    header_data->response->headerLength = header_data->entries.size();
    header_data->response->status = 200;
    header_data->response->url = nullptr;
    header_data->response->httpVersion = nullptr;
    header_data->response->method = nullptr;
    // header finish
    header_data->callback(header_data->response);
    std::cout << "Header Finished" << std::endl;
    return total_size;
  }
  if (!header_data->first_line) {
    header_data->first_line = header_line;
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
  header_data->entries.push_back(Field{.key = key, .value = value});
  fflush(stdout);

  return total_size;
}

void sendRequest(Config *config, Request *request, ResponseCallback callback,
                 DataHandler onData, ErrorHandler onError) {
  std::cout << "send Request" << std::endl;
  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    onError("Unable to initialize curl");
    return;
  }

  auto *data = new HeaderCallbackData();
  data->response = new Response();
  data->callback = callback;
  data->first_line = nullptr;

  auto *body = new BodyCallbackData();
  body->callback = onData;

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);
  // 设置回调函数，用于处理响应数据
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);

  std::string url = "http://example.com";
  CURLcode ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (ret != CURLE_OK) {
    onError("Unable to set URL");
    return;
  }

  // 发送请求并获取响应
  ret = curl_easy_perform(curl);
  onData({nullptr, 0});

  delete data;
  delete body;
  // 检查请求是否成功
  if (ret != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(ret));
  } else {
  }
}

void flucurl_free_reponse(Response *p) {
  for (int i = 0; i < p->headerLength; i++) {
    delete[] p->header[i].key;
    delete[] p->header[i].value;
  }
  delete[] p->header;
  delete p;
}
void flucurl_free_bodydata(const char *p) { delete[] p; }