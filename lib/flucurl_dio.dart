import 'dart:typed_data';
import 'package:flucurl/src/binding.dart';

import 'flucurl.dart';
import 'package:dio/dio.dart';

abstract class Flucurl {
  static init() {
    bindings.init();
  }
}

class FlucurlAdapter implements HttpClientAdapter {
  late final FlucurlClient client;

  FlucurlAdapter({FlucurlConfig config = const FlucurlConfig(), BaseOptions? options}) {
    client = FlucurlClient(config: config);
    options = options ?? BaseOptions();
  }

  @override
  void close({bool force = false}) {
    client.close();
  }

  @override
  Future<ResponseBody> fetch(RequestOptions options, Stream<Uint8List>? requestStream, Future<void>? cancelFuture) async {
    var response = await client.send(Request(
      url: options.uri.toString(),
      method: options.method,
      headers: options.headers.map((key, value) => MapEntry(key, value.toString())),
      body: requestStream,
    ));

    return ResponseBody(
      response.body,
      response.statusCode,
      headers: response.headers,
    );
  }
}

class FlucurlDio with DioMixin {
  FlucurlDio({FlucurlConfig config = const FlucurlConfig()}) {
    httpClientAdapter = FlucurlAdapter(config: config);
  }
}