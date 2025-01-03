import 'package:flucurl/src/binding.dart';

export 'src/types.dart';
export 'src/client.dart';

class Flucurl {
  static init() {
    bindings.init();
  }
}