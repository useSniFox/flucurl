import 'dart:typed_data';
import 'flucurl_bindings_generated.dart' as generated;

typedef HttpVersion = generated.HTTPVersion;

class FlucurlConfig {
  final int timeout;

  final String proxy;

  final String? Function(String host)? dnsResolver;

  final TlsConfig tlsConfig;

  final HttpVersion httpVersion;

  final bool keepAlive;

  final int idleTimeout;

  const FlucurlConfig({
    this.timeout = 30,
    this.proxy = '',
    this.dnsResolver,
    this.tlsConfig = const TlsConfig(),
    this.httpVersion = HttpVersion.HTTP2,
    this.keepAlive = true,
    this.idleTimeout = 120,
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

class FlucurlRequest {
  final String url;

  final String method;

  final Map<String, String> headers;

  final Object? body;

  FlucurlRequest({
    required this.url,
    this.method = 'GET',
    Map<String, String>? headers,
    this.body,
  }): headers = headers ?? {};

  FlucurlRequest copyWith({
    String? url,
    String? method,
    Map<String, String>? headers,
    Object? body,
  }) {
    return FlucurlRequest(
      url: url ?? this.url,
      method: method ?? this.method,
      headers: headers ?? this.headers,
      body: body ?? this.body,
    );
  }
}

class FlucurlResponse {
  final String url;

  final String method;

  final int statusCode;

  final Map<String, List<String>> headers;

  final Stream<Uint8List> body;

  FlucurlResponse({
    required this.url,
    required this.method,
    required this.statusCode,
    required this.headers,
    required this.body,
  });
}