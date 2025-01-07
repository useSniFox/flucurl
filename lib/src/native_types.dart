import 'package:flucurl/src/types.dart';
import 'package:flucurl/src/flucurl_bindings_generated.dart' as bindings;
import 'dart:ffi' as ffi;
import 'package:flucurl/src/utils.dart';

class NativeConfig with NativeFreeable {
  final FlucurlConfig config;
  late final ffi.Pointer<bindings.Config> nativeConfig;

  static ffi.NativeCallable<ffi.Void Function(ffi.Pointer<ffi.Void>)>? _freeDartMemory;

  NativeConfig(this.config) {
    nativeConfig = allocate(ffi.sizeOf<bindings.Config>());
    nativeConfig.ref.timeout = config.timeout;
    nativeConfig.ref.proxy = config.proxy == '' ? ffi.nullptr.cast() : config.proxy.toNative(this);
    nativeConfig.ref.tls_config = allocate(ffi.sizeOf<bindings.TLSConfig>());
    nativeConfig.ref.tls_config.ref.enable_sni = config.tlsConfig.sni ? 1 : 0;
    nativeConfig.ref.tls_config.ref.verify_certificates = config.tlsConfig.verifyCertificates ? 1 : 0;
    var cas = allocate<ffi.Pointer<ffi.Char>>(ffi.sizeOf<ffi.Pointer>() * config.tlsConfig.trustedRootCertificates.length);
    for (int i = 0; i < config.tlsConfig.trustedRootCertificates.length; i++) {
      cas[i] = config.tlsConfig.trustedRootCertificates[i].toNative(this);
    }
    nativeConfig.ref.idle_timeout = config.idleTimeout;
    nativeConfig.ref.keep_alive = config.keepAlive ? 1 : 0;
    nativeConfig.ref.http_version = config.httpVersion.index;
    _freeDartMemory ??= ffi.NativeCallable.listener(NativeFreeable.freePtr);
    nativeConfig.ref.free_dart_memory = _freeDartMemory!.nativeFunction;
  }
}

class NativeRequest with NativeFreeable {
  final FlucurlRequest request;
  late final ffi.Pointer<bindings.Request> nativeRequest;

  final Map<HeaderKey, String> headers = {};

  NativeRequest(this.request, String? resolvedIP) {
    getHeaders(request.headers);
    nativeRequest = allocate(ffi.sizeOf<bindings.Request>());
    nativeRequest.ref.url = request.url.toNative(this);
    nativeRequest.ref.method = request.method.toNative(this);
    nativeRequest.ref.headers = allocate(ffi.sizeOf<ffi.Pointer>() * headers.length);
    int i = 0;
    for (var entry in headers.entries) {
      var value = "${entry.key}: ${entry.value}";
      nativeRequest.ref.headers[i] = value.toNative(this);
      i++;
    }
    nativeRequest.ref.content_length = contentSize;
    nativeRequest.ref.header_count = headers.length;
    nativeRequest.ref.resolved_ip = resolvedIP == null ? ffi.nullptr.cast() : resolvedIP.toNative(this);
  }

  void getHeaders(Map<String, String> reqHeaders) {
    for (var key in reqHeaders.keys) {
      // prevent duplicate headers
      headers[HeaderKey(key)] ??= reqHeaders[key]!;
    }
    headers[HeaderKey('User-Agent')] ??= "Dart with Flucurl";
  }

  int get contentSize {
    if (headers.containsKey(HeaderKey('Content-Length'))) {
      return int.parse(headers[HeaderKey('Content-Length')]!);
    } else {
      return 0;
    }
  }
}

class HeaderKey {
  final String key;
  
  HeaderKey(this.key);

  @override
  operator ==(Object other) {
    if (other is HeaderKey) {
      return key.toLowerCase() == other.key.toLowerCase();
    } else {
      return false;
    }
  }

  @override
  int get hashCode => key.toLowerCase().hashCode;

  @override
  String toString() => key;
}