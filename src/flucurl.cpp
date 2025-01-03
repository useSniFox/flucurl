#include "flucurl.h"
#include <curl/curl.h>
#include <iostream>

void init() {
  curl_easy_init();
  std::cout << "Curl initialized" << std::endl;
}