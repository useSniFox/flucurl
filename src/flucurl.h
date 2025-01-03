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
  int id;
  char *url;
  char *method;
  char *data;
  int contentLength;
  Field *header;
  int headerLength;
} Request;

typedef struct Response {
  char *url;
  char *method;
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

  /// Maximum number of redirects to follow.
  int maxRedirect;

  /// http or socks5 proxy, in the format of "http://host:port" or
  /// "socks5://host:port". Null for no proxy.
  char *proxy;

  /// DNS resolver function. If null or returns null, the system default
  /// resolver will be used.
  DnsResolver *dnsResolver;

  /// TLS configuration.
  TlsConfig *tlsConfig;
} Config;

typedef void (*ResponseCallback)(int id, Response *);

typedef void (*DataHandler)(int id, const char *data, int length);

typedef void (*ErrorHandler)(int id, const char *message);

FFI_PLUGIN_EXPORT void init();

FFI_PLUGIN_EXPORT void sendRequest(Config *config, Request *request,
                                   ResponseCallback callback,
                                   DataHandler onData, ErrorHandler onError);
#ifdef __cplusplus
}
#endif