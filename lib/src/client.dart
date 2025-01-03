import 'package:flucurl/src/native_types.dart';
import 'package:flucurl/src/types.dart';

class FlucurlClient {
  final FlucurlConfig config;

  FlucurlClient({
    this.config = const FlucurlConfig(),
  });

  Future<Response> send(Request request) async {
    var nativeConfig = NativeConfig(config);
    var nativeRequest = NativeRequest(request);
    throw UnimplementedError();
  }
}

