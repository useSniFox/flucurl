import 'package:flucurl/src/binding.dart';

export 'src/types.dart';
export 'src/client.dart';

abstract class Flucurl {
  static init() {
    bindings.init();
  }
}