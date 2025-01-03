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
  char *url;
  char *method;
  char *data;
  int contentLength;
  Field *header;
  int headerLength;
} Request;

typedef struct Response {
  const char *httpVersion;
  int status;
  const char *url;
  const char *method;
  Field *header;
  int headerLength;
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

typedef void (*ResponseCallback)(Response *);

typedef void (*DataHandler)(const BodyData);

typedef void (*ErrorHandler)(const char *message);

FFI_PLUGIN_EXPORT void init();

FFI_PLUGIN_EXPORT void flucurl_free_reponse(Response *);
FFI_PLUGIN_EXPORT void flucurl_free_bodydata(const char *);

FFI_PLUGIN_EXPORT void sendRequest(Config *config, Request *request,
                                   ResponseCallback callback,

                                   DataHandler onData, ErrorHandler onError);
#ifdef __cplusplus
}
#endif