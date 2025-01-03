#include "flucurl.h"

#include <cstdio>
#include <iostream>
#include <ostream>

#include "build/vcpkg_installed/x64-windows/include/curl/curl.h"
#include "build/vcpkg_installed/x64-windows/include/curl/easy.h"

void init() {
  int ret = curl_global_init(CURL_GLOBAL_ALL);
  if (ret != CURLE_OK) {
    std::cout << "Unable to initialize curl" << std::endl;
  } else {
    std::cout << "Curl initialized" << std::endl;
  }
}

// 回调函数，用于处理响应数据
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  // 将响应数据写入缓冲区
  printf("%.*s", (int)total_size, (char *)ptr);
  fflush(stdout);
  return total_size;
}

// 回调函数：处理 HTTP 响应头
size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  char *header_line = (char *)malloc(total_size + 1);
  if (header_line) {
    // 将响应头数据拷贝到本地
    memcpy(header_line, ptr, total_size);
    header_line[total_size] = '\0';  // 确保是一个以 '\0' 结尾的字符串

    // 打印响应头
    printf("Header: %s", header_line);

    fflush(stdout);
    free(header_line);
  }

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

  // 设置回调函数，用于处理响应数据
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

  std::string url = "http://example.com";
  CURLcode ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (ret != CURLE_OK) {
    onError("Unable to set URL");
    return;
  }

  // 发送请求并获取响应
  ret = curl_easy_perform(curl);

  // 检查请求是否成功
  if (ret != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(ret));
  }
}

void flucurl_free_reponse(Response *p) { delete p; }
void flucurl_free_bodydata(BodyData *p) { delete p; }