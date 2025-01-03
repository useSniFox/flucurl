import 'dart:ffi' as ffi;

import 'package:flucurl/src/binding.dart';
import 'package:flucurl/src/flucurl_bindings_generated.dart';

class Flucurl {
  static init() {
    bindings.init();
  }
  void test(){
    bindings.sendRequest(ffi.nullptr,ffi.nullptr,ffi.nullptr,ffi.nullptr,ffi.nullptr);
  }
}