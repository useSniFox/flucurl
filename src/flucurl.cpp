#include "flucurl.h"

#include <iostream>

#include "build/vcpkg_installed/x64-windows/include/curl/curl.h"


void init() {
  int ret = curl_global_init(CURL_GLOBAL_ALL);
  if (ret != 0) {
    std::cout << "Unable to initialize curl" << std::endl;
  } else {
    std::cout << "Curl initialized" << std::endl;
  }
}

void sendRequest(Config *config, Request *request, ResponseCallback callback,
                 DataHandler onData, ErrorHandler onError) {}