import 'dart:typed_data';

class FlucurlConfig {
  final int timeout;

  final String proxy;

  final String? Function(String host)? dnsResolver;

  final TlsConfig tlsConfig;

  const FlucurlConfig({
    this.timeout = 30000,
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

  Request copyWith({
    String? url,
    String? method,
    Map<String, String>? headers,
    Object? body,
  }) {
    return Request(
      url: url ?? this.url,
      method: method ?? this.method,
      headers: headers ?? this.headers,
      body: body ?? this.body,
    );
  }
}

class Response {
  final String url;

  final String method;

  final int statusCode;

  final Map<String, List<String>> headers;

  final Stream<Uint8List> body;

  Response({
    required this.url,
    required this.method,
    required this.statusCode,
    required this.headers,
    required this.body,
  });
}