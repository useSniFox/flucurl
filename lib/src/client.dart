import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:flucurl/src/binding.dart';
import 'package:flucurl/src/native_types.dart';
import 'package:flucurl/src/types.dart';
import 'package:flucurl/src/flucurl_bindings_generated.dart' as generated;

class FlucurlClient {
  late ffi.Pointer<ffi.Void> session;

  FlucurlClient({
    FlucurlConfig config = const FlucurlConfig(),
  }) {
    var nativeConfig = NativeConfig(config);
    session = bindings.session_init(nativeConfig.nativeConfig.ref);
    nativeConfig.free();
  }

  Future<Response> send(Request request) async {
    var req = NativeRequest(request);
    var completer = Completer<Response>();
    var bodyStreamController = StreamController<Uint8List>();

    if (request.body is Stream<List<int>> ||
        request.body is Stream<Uint8List>) {
      var data = <int>[];
      await for (var chunk in request.body as Stream<List<int>>) {
        data.addAll(chunk);
      }
      request = request.copyWith(body: data);
    }

    var nativeFunctions = <ffi.NativeCallable>[];

    void clear() {
      print('clearing');
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
        var key = field.ref.key.cast<Utf8>().toDartString();
        var value = field.ref.value.cast<Utf8>().toDartString();
        headers.putIfAbsent(key, () => []).add(value);
      }
      bindings.flucurl_free_reponse(response);
      completer.complete(Response(
        url: url,
        method: method,
        statusCode: statusCode,
        headers: headers,
        body: bodyStreamController.stream,
      ));
    }

    void onData(generated.BodyData data) {
      if (data.data == ffi.nullptr) {
        bodyStreamController.close();
        clear();
        return;
      }
      var view = data.data
          .cast<ffi.Uint8>()
          .asTypedList(data.size, finalizer: nativeFreeBodyDataFunction.cast());
      bodyStreamController.add(view);
    }

    void onError(ffi.Pointer<ffi.Char> error) {
      clear();
      var message = error.cast<Utf8>().toDartString();
      if (completer.isCompleted) {
        bodyStreamController.addError(message);
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

    bindings.session_send_request(
      session,
      req.nativeRequest.ref,
      nativeResponseCallback.nativeFunction,
      nativeDataHandler.nativeFunction,
      nativeErrorHandler.nativeFunction,
    );

    return completer.future;
  }

  void close() {
    bindings.session_terminate(session);
  }
}
