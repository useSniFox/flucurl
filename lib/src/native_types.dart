import 'dart:convert';

import 'package:ffi/ffi.dart';
import 'package:flucurl/src/types.dart';
import 'package:flucurl/src/flucurl_bindings_generated.dart' as bindings;
import 'dart:ffi' as ffi;
import 'package:flucurl/src/utils.dart';

class NativeConfig with NativeFreeable {
  final FlucurlConfig config;
  late final ffi.Pointer<bindings.Config> nativeConfig;

  NativeConfig(this.config) {
    nativeConfig = allocate(ffi.sizeOf<bindings.Config>());
    nativeConfig.ref.timeout = config.timeout;
    nativeConfig.ref.proxy = config.proxy.toNative(this);
    if (config.dnsResolver != null) {
      ffi.Pointer<ffi.Char> resolver(ffi.Pointer<ffi.Char> host) {
        var result = config.dnsResolver!(host.cast<Utf8>().toDartString());
        if (result == null) {
          return ffi.nullptr.cast();
        } else {
          var p = result.toNative(this);
          return p;
        }
      }

      var callback = ffi.NativeCallable<bindings.DnsResolverFunction>.isolateLocal(resolver);
      nativeConfig.ref.dnsResolver = callback.nativeFunction.cast();
      addFunction(callback);
    }
    nativeConfig.ref.tlsConfig = allocate(ffi.sizeOf<bindings.TlsConfig>());
    nativeConfig.ref.tlsConfig.ref.sni = config.tlsConfig.sni ? 1 : 0;
    nativeConfig.ref.tlsConfig.ref.verifyCertificates = config.tlsConfig.verifyCertificates ? 1 : 0;
    var cas = allocate<ffi.Pointer<ffi.Char>>(ffi.sizeOf<ffi.Pointer>() * config.tlsConfig.trustedRootCertificates.length);
    for (int i = 0; i < config.tlsConfig.trustedRootCertificates.length; i++) {
      cas[i] = config.tlsConfig.trustedRootCertificates[i].toNative(this);
    }
  }
}

class NativeRequest with NativeFreeable {
  final Request request;
  late final ffi.Pointer<bindings.Request> nativeRequest;

  NativeRequest(this.request) {
    nativeRequest = allocate(ffi.sizeOf<bindings.Request>());
    nativeRequest.ref.url = request.url.toNative(this);
    nativeRequest.ref.method = request.method.toNative(this);
    var reqHeaders = request.headers;
    var (data, size) = _translateBody(request.body, reqHeaders);
    var headers = getHeaders(reqHeaders);
    nativeRequest.ref.header = allocate(ffi.sizeOf<bindings.Field>() * headers.length);
    for (int i = 0; i < headers.length; i++) {
      nativeRequest.ref.header[i].key = headers.keys.elementAt(i).toNative(this);
      nativeRequest.ref.header[i].value = headers.values.elementAt(i).toNative(this);
    }
    nativeRequest.ref.data = data;
    nativeRequest.ref.contentLength = size;
  }

  Map<String, String> getHeaders(Map<String, String> reqHeaders) {
    var headers = <HeaderKey, String>{};
    for (var key in reqHeaders.keys) {
      // prevent duplicate headers
      headers[HeaderKey(key)] ??= reqHeaders[key]!;
    }
    headers[HeaderKey('User-Agent')] ??= "Dart with Flucurl";
    var result = <String, String>{};
    for (var key in headers.keys) {
      result[key.key] = headers[key]!;
    }
    return result;
  }

  (ffi.Pointer<ffi.Char>, int) _translateBody(Object? body, Map<String, String> headers) {
    if (body == null) {
      return (ffi.nullptr.cast(), 0);
    } else if (body is String) {
      var data = utf8.encode(body);
      var p = allocate<ffi.Uint8>(data.length);
      for (int i = 0; i < data.length; i++) {
        p[i] = data[i];
      }
      headers['Content-Type'] ??= 'text/plain';
      headers['Content-Length'] = data.length.toString();
      return (p.cast(), data.length);
    } else if (body is List<int>) {
      var p = allocate<ffi.Uint8>(body.length);
      for (int i = 0; i < body.length; i++) {
        p[i] = body[i];
      }
      headers['Content-Type'] ??= 'application/octet-stream';
      headers['Content-Length'] = body.length.toString();
      return (p.cast(), body.length);
    } else if (body is Map<String, String>) {
      var data = utf8.encode(json.encode(body));
      var p = allocate<ffi.Uint8>(data.length);
      for (int i = 0; i < data.length; i++) {
        p[i] = data[i];
      }
      headers['Content-Type'] ??= 'application/json';
      headers['Content-Length'] = data.length.toString();
      return (p.cast(), data.length);
    } else if (body is Stream<List<int>>) {
      // The request body should be converted to Uint8List before creating the request object
      throw ArgumentError('Stream body is not supported');
    } else {
      throw ArgumentError('Invalid body type');
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
}