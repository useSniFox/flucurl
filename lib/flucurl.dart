import 'dart:ffi' as ffi;

import 'package:flucurl/src/binding.dart';

class Flucurl {
  static init() {
    bindings.init();
  }
  void test(){
    bindings.sendRequest(ffi.nullptr,ffi.nullptr,ffi.nullptr,ffi.nullptr,ffi.nullptr);
  }
}