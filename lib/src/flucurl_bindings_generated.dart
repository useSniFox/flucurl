// ignore_for_file: always_specify_types
// ignore_for_file: camel_case_types
// ignore_for_file: non_constant_identifier_names

// AUTO GENERATED FILE, DO NOT EDIT.
//
// Generated by `package:ffigen`.
// ignore_for_file: type=lint
import 'dart:ffi' as ffi;

/// Bindings for `src/flucurl.h`.
///
/// Regenerate bindings with `dart run ffigen --config ffigen.yaml`.
///
class FlucurlBindings {
  /// Holds the symbol lookup function.
  final ffi.Pointer<T> Function<T extends ffi.NativeType>(String symbolName)
      _lookup;

  /// The symbols are looked up in [dynamicLibrary].
  FlucurlBindings(ffi.DynamicLibrary dynamicLibrary)
      : _lookup = dynamicLibrary.lookup;

  /// The symbols are looked up with [lookup].
  FlucurlBindings.fromLookup(
      ffi.Pointer<T> Function<T extends ffi.NativeType>(String symbolName)
          lookup)
      : _lookup = lookup;

  void init() {
    return _init();
  }

  late final _initPtr =
      _lookup<ffi.NativeFunction<ffi.Void Function()>>('init');
  late final _init = _initPtr.asFunction<void Function()>();

  void flucurl_free_reponse(
    ffi.Pointer<Response> response,
  ) {
    return _flucurl_free_reponse(
      response,
    );
  }

  late final _flucurl_free_reponsePtr =
      _lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<Response>)>>(
          'flucurl_free_reponse');
  late final _flucurl_free_reponse = _flucurl_free_reponsePtr
      .asFunction<void Function(ffi.Pointer<Response>)>();

  void flucurl_free_bodydata(
    ffi.Pointer<BodyData> bodyData,
  ) {
    return _flucurl_free_bodydata(
      bodyData,
    );
  }

  late final _flucurl_free_bodydataPtr =
      _lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<BodyData>)>>(
          'flucurl_free_bodydata');
  late final _flucurl_free_bodydata = _flucurl_free_bodydataPtr
      .asFunction<void Function(ffi.Pointer<BodyData>)>();

  void sendRequest(
    ffi.Pointer<Config> config,
    ffi.Pointer<Request> request,
    ResponseCallback callback,
    DataHandler onData,
    ErrorHandler onError,
  ) {
    return _sendRequest(
      config,
      request,
      callback,
      onData,
      onError,
    );
  }

  late final _sendRequestPtr = _lookup<
      ffi.NativeFunction<
          ffi.Void Function(ffi.Pointer<Config>, ffi.Pointer<Request>,
              ResponseCallback, DataHandler, ErrorHandler)>>('sendRequest');
  late final _sendRequest = _sendRequestPtr.asFunction<
      void Function(ffi.Pointer<Config>, ffi.Pointer<Request>, ResponseCallback,
          DataHandler, ErrorHandler)>();
}

final class Field extends ffi.Struct {
  external ffi.Pointer<ffi.Char> key;

  external ffi.Pointer<ffi.Char> value;
}

final class Request extends ffi.Struct {
  external ffi.Pointer<ffi.Char> url;

  external ffi.Pointer<ffi.Char> method;

  external ffi.Pointer<ffi.Char> data;

  @ffi.Int()
  external int contentLength;

  external ffi.Pointer<Field> header;

  @ffi.Int()
  external int headerLength;
}

final class Response extends ffi.Struct {
  external ffi.Pointer<ffi.Char> httpVersion;

  @ffi.Int()
  external int status;

  external ffi.Pointer<ffi.Char> url;

  external ffi.Pointer<ffi.Char> method;

  external ffi.Pointer<Field> header;

  @ffi.Int()
  external int headerLength;
}

final class TlsConfig extends ffi.Struct {
  /// Enable certificate verification.
  @ffi.Int()
  external int verifyCertificates;

  /// Enable TLS Server Name Indication (SNI).
  @ffi.Int()
  external int sni;

  /// The trusted root certificates in PEM format.
  /// Either specify the root certificate or the full
  /// certificate chain.
  /// The Rust API currently doesn't support trusting a single leaf certificate.
  /// Hint: PEM format starts with `-----BEGIN CERTIFICATE-----`.
  external ffi.Pointer<ffi.Pointer<ffi.Char>> trustedRootCertificates;

  @ffi.Int()
  external int trustedRootCertificatesLength;
}

final class Config extends ffi.Struct {
  /// Timeout in seconds.
  @ffi.Int()
  external int timeout;

  /// Maximum number of redirects to follow.
  @ffi.Int()
  external int maxRedirect;

  /// http or socks5 proxy, in the format of "http://host:port" or
  /// "socks5://host:port". Null for no proxy.
  external ffi.Pointer<ffi.Char> proxy;

  /// DNS resolver function. If null or returns null, the system default
  /// resolver will be used.
  external ffi.Pointer<DnsResolver> dnsResolver;

  /// TLS configuration.
  external ffi.Pointer<TlsConfig> tlsConfig;
}

typedef DnsResolver = ffi.Pointer<ffi.NativeFunction<DnsResolverFunction>>;
typedef DnsResolverFunction = ffi.Pointer<ffi.Char> Function(
    ffi.Pointer<ffi.Char> host);

final class BodyData extends ffi.Struct {
  external ffi.Pointer<ffi.Char> data;

  @ffi.Int()
  external int size;
}

typedef ResponseCallback
    = ffi.Pointer<ffi.NativeFunction<ResponseCallbackFunction>>;
typedef ResponseCallbackFunction = ffi.Void Function(ffi.Pointer<Response>);
typedef DartResponseCallbackFunction = void Function(ffi.Pointer<Response>);
typedef DataHandler = ffi.Pointer<ffi.NativeFunction<DataHandlerFunction>>;
typedef DataHandlerFunction = ffi.Void Function(ffi.Pointer<BodyData>);
typedef DartDataHandlerFunction = void Function(ffi.Pointer<BodyData>, int);
typedef ErrorHandler = ffi.Pointer<ffi.NativeFunction<ErrorHandlerFunction>>;
typedef ErrorHandlerFunction = ffi.Void Function(ffi.Pointer<ffi.Char> message);
typedef DartErrorHandlerFunction = void Function(ffi.Pointer<ffi.Char> message);
