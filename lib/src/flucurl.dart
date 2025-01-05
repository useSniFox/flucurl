import 'package:flucurl/src/binding.dart';

abstract class Flucurl {
  static init() {
    bindings.flucurl_global_init();
  }
}