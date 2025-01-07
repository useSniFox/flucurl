import 'dart:async';
import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:flucurl/src/binding.dart';
import 'package:flucurl/src/native_types.dart';
import 'package:flucurl/src/types.dart';
import 'package:flucurl/src/flucurl_bindings_generated.dart' as generated;
import 'package:flucurl/src/utils.dart';

typedef _DNSResolver = String? Function(String host);

class FlucurlClient {
  late ffi.Pointer<ffi.Void> session;

  _DNSResolver? _dnsResolver;

  FlucurlClient({
    FlucurlConfig config = const FlucurlConfig(),
  }) {
    _dnsResolver = config.dnsResolver;
    var nativeConfig = NativeConfig(config);
    session = bindings.flucurl_session_init(nativeConfig.nativeConfig.ref);
    nativeConfig.free();
  }

  FlucurlRequest _translateRequestBody(FlucurlRequest request) {
    if (request.body is String) {
      request.headers['Content-Type'] ??= 'text/plain';
      var data = utf8.encode(request.body as String);
      request.headers['Content-Length'] ??= data.length.toString();
      return request.copyWith(body: Stream.value(Uint8List.fromList(data)));
    } else if (request.body is List<int>) {
      request.headers['Content-Length'] ??= (request.body as List<int>).length.toString();
      return request.copyWith(body: Stream.value(Uint8List.fromList(request.body as List<int>)));
    } else if (request.body is Stream<List<int>> || request.body is Stream<Uint8List>) {
      if (request.headers['Content-Length'] == null || request.headers['content-length'] == null) {
        throw ArgumentError('Content-Length must be provided for Stream<List<int>>');
      }
      return request;
    } else if (request.body is Map || request.body is List) {
      request.headers['Content-Type'] ??= 'application/json';
      var data = utf8.encode(json.encode(request.body));
      request.headers['Content-Length'] ??= data.length.toString();
      return request.copyWith(body: Stream.value(Uint8List.fromList(data)));
    } else if (request.body == null) {
      request.headers['Content-Length'] ??= '0';
      return request;
    } else {
      throw ArgumentError('Invalid body type');
    }
  }

  Future<FlucurlResponse> send(FlucurlRequest request) async {
    request = _translateRequestBody(request);

    var req = NativeRequest(request, _dnsResolver?.call(request.url));
    var completer = Completer<FlucurlResponse>();
    var bodySink = StreamController<Uint8List>();

    var nativeFunctions = <ffi.NativeCallable>[];

    void clear() {
      for (var function in nativeFunctions) {
        function.close();
      }
      req.free();
    }

    void onResponse(generated.Response response) {
      var url = request.url;
      var method = request.method;
      var statusCode = response.status;
      var headers = <String, List<String>>{};
      for (int i = 0; i < response.header_count; i++) {
        var field = ffi.Pointer<generated.Field>.fromAddress(
            response.headers.address + i * ffi.sizeOf<generated.Field>());
        var data = field.ref.p.cast<ffi.Uint8>().asTypedList(field.ref.len);
        var str = utf8.decode(data);
        if (!str.contains(':')) {
          continue;
        }
        var spliter = str.indexOf(':');
        var key = str.substring(0, spliter);
        var value = str.substring(spliter + 1).trim();
        headers[key] ??= [];
        headers[key]!.add(value);
      }
      bindings.flucurl_free_reponse(response);
      completer.complete(FlucurlResponse(
        url: url,
        method: method,
        statusCode: statusCode,
        headers: headers,
        body: bodySink.stream,
      ));
    }

    void onData(generated.BodyData data) {
      try {
        if (data.data == ffi.nullptr) {
          bodySink.close();
          clear();
          return;
        }
        bodySink.add(Uint8List.fromList(data.data.cast<ffi.Uint8>().asTypedList(data.size)));
      } finally {
        bindings.flucurl_free_bodydata(data);
      }
    }

    void onError(ffi.Pointer<ffi.Char> error) {
      clear();
      var message = error.cast<Utf8>().toDartString();
      if (completer.isCompleted) {
        bodySink.addError(message);
      } else {
        completer.completeError(message);
      }
    }

    var nativeResponseCallback =
        ffi.NativeCallable<generated.ResponseCallbackFunction>.listener(
            onResponse);
    var nativeDataHandler =
        ffi.NativeCallable<generated.DataHandlerFunction>.listener(onData);
    var nativeErrorHandler =
        ffi.NativeCallable<generated.ErrorHandlerFunction>.listener(onError);

    nativeFunctions.addAll(
        [nativeResponseCallback, nativeDataHandler, nativeErrorHandler]);
        
    var state = bindings.flucurl_session_send_request(
      session,
      req.nativeRequest.ref,
      nativeResponseCallback.nativeFunction,
      nativeDataHandler.nativeFunction,
      nativeErrorHandler.nativeFunction,
    );

    if (request.body == null) {
      return completer.future;
    }

    const bufferSize = 4 * 1024;
    var buffer = NativeFreeable.allocateMem<ffi.Uint8>(bufferSize);
    int writeIndex = 0;

    await for (var d in request.body as Stream) {
      assert(d is List<int>);
      var data = d as List<int>;
      int readIndex = 0;
      while(readIndex != data.length) {
        var bufferAvailable = bufferSize - writeIndex;
        var dataAvailable = data.length - readIndex;
        var toWrite = bufferAvailable < dataAvailable ? bufferAvailable : dataAvailable;
        (buffer+writeIndex).asTypedList(toWrite).setAll(0, data.getRange(readIndex, readIndex+toWrite));
        writeIndex += toWrite;
        readIndex += toWrite;
        if (writeIndex == bufferSize) {
          var field = ffi.Struct.create<generated.Field>();
          field.p = buffer.cast();
          field.len = bufferSize;
          bindings.flucurl_upload_append(state.ref, field);
          writeIndex = 0;
        }
      }
    }

    if (writeIndex != 0) {
      var field = ffi.Struct.create<generated.Field>();
      field.p = buffer.cast();
      field.len = writeIndex;
      bindings.flucurl_upload_append(state.ref, field);
    }

    return completer.future;
  }

  void close() {
    bindings.flucurl_session_terminate(session);
  }
}
