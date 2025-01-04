#pragma once

#if _WIN32
#define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FFI_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef struct Field {
  char *key;
  char *value;
} Field;

typedef struct Request {
  const char *url;
  const char *method;
  const char *data;
  int content_length;
  Field *headers;
  int header_count;
} Request;

typedef struct Response {
  const char *http_version;
  int status;
  Field *headers;
  int header_count;
} Response;

typedef char *(*DnsResolver)(const char *host);

typedef struct TlsConfig {
  /// Enable certificate verification.
  int verifyCertificates;

  /// Enable TLS Server Name Indication (SNI).
  int sni;

  /// The trusted root certificates in PEM format.
  /// Either specify the root certificate or the full
  /// certificate chain.
  /// The Rust API currently doesn't support trusting a single leaf certificate.
  /// Hint: PEM format starts with `-----BEGIN CERTIFICATE-----`.
  const char **trustedRootCertificates;

  int trustedRootCertificatesLength;
} TlsConfig;

typedef struct Config {
  /// Timeout in seconds.
  int timeout;

  /// http or socks5 proxy, in the format of "http://host:port" or
  /// "socks5://host:port". Null for no proxy.
  char *proxy;

  /// DNS resolver function. If null or returns null, the system default
  /// resolver will be used.
  DnsResolver *dnsResolver;

  /// TLS configuration.
  TlsConfig *tlsConfig;
} Config;

typedef struct BodyData {
  const char *data;
  int size;
} BodyData;

typedef void (*ResponseCallback)(Response);

typedef void (*DataHandler)(const BodyData);

typedef void (*ErrorHandler)(const char *message);

FFI_PLUGIN_EXPORT void global_init();

FFI_PLUGIN_EXPORT void flucurl_free_reponse(Response);
FFI_PLUGIN_EXPORT void flucurl_free_bodydata(const char *);

FFI_PLUGIN_EXPORT void sendRequest(Request request, ResponseCallback callback,

                                   DataHandler onData, ErrorHandler onError);

FFI_PLUGIN_EXPORT void *session_init(Config config);
FFI_PLUGIN_EXPORT void session_terminate(void *session);
FFI_PLUGIN_EXPORT void session_send_request(void *session, Request request,
                                            ResponseCallback callback,
                                            DataHandler onData,
                                            ErrorHandler onError);
#ifdef __cplusplus
}
#endif