import 'package:flucurl/src/binding.dart';

abstract class Flucurl {
  static init() {
    bindings.global_init();
  }
}