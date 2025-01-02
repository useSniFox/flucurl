#pragma once

#if _WIN32
#define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FFI_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
  struct Field {
    char* key;
    char* value;
  };

  struct Request {
    char* url;
    char* method;
    char* data;
    int contentLength;
    struct Field* header;
    int headerLength;
  };

  struct Response {
    char* url;
    char* method;
    char* data;
    int contentLength;
    struct Field* header;
    int headerLength;
  };

  typedef void (*RequestCallback)(struct Request*, struct Response*);

  FFI_PLUGIN_EXPORT void init();

  FFI_PLUGIN_EXPORT void sendRequest(struct Request* request, RequestCallback callback);
#ifdef __cplusplus
}
#endif