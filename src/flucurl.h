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
  char *key;
  char *value;
};

struct Request {
  int id;
  char *url;
  char *method;
  char *data;
  int contentLength;
  struct Field *header;
  int headerLength;
};

struct Response {
  char *url;
  char *method;
  struct Field *header;
  int headerLength;
};

typedef char *(*DnsResolver)(const char *host);

struct Config {
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
  struct TlsConfig *tlsConfig;
};

struct TlsConfig {
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
};

typedef void (*ResponseCallback)(int id, struct Response *);

typedef void (*DataHandler)(int id, const char *data, int length);

typedef void (*ErrorHandler)(int id, const char *message);

FFI_PLUGIN_EXPORT void init();

FFI_PLUGIN_EXPORT void sendRequest(struct Config *config,
                                   struct Request *request,
                                   ResponseCallback callback,
                                   DataHandler onData, ErrorHandler onError);
#ifdef __cplusplus
}
#endif