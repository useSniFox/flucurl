class FlucurlConfig {
  final int timeout;

  final int maxRedirects;

  final String proxy;

  final String? Function(String host)? dnsResolver;

  final TlsConfig tlsConfig;

  const FlucurlConfig({
    this.timeout = 30000,
    this.maxRedirects = 5,
    this.proxy = '',
    this.dnsResolver,
    this.tlsConfig = const TlsConfig(),
  });
}

class TlsConfig {
  final bool verifyCertificates;

  final bool sni;

  final List<String> trustedRootCertificates;

  const TlsConfig({
    this.verifyCertificates = true,
    this.sni = true,
    this.trustedRootCertificates = const [],
  });
}

class Request {
  final String url;

  final String method;

  final Map<String, String> headers;

  final Object? body;

  const Request({
    required this.url,
    this.method = 'GET',
    this.headers = const {},
    this.body,
  });
}

class Response {
  
}