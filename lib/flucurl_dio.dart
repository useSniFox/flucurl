import 'dart:typed_data';
import 'flucurl.dart';
import 'package:dio/dio.dart';

export 'src/flucurl.dart';

class FlucurlAdapter implements HttpClientAdapter {
  late final FlucurlClient client;

  FlucurlAdapter({FlucurlConfig config = const FlucurlConfig()}) {
    client = FlucurlClient(config: config);
  }

  @override
  void close({bool force = false}) {
    client.close();
  }

  @override
  Future<ResponseBody> fetch(RequestOptions options, Stream<Uint8List>? requestStream, Future<void>? cancelFuture) async {
    var response = await client.send(FlucurlRequest(
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
  FlucurlDio({FlucurlConfig config = const FlucurlConfig(), BaseOptions? baseOptions}) {
    httpClientAdapter = FlucurlAdapter(config: config);
    options = baseOptions ?? BaseOptions();
  }
}