import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';

mixin class NativeFreeable {
  final _pointers = <ffi.Pointer>[];
  final _nativeFunctions = <ffi.NativeCallable>[];

  void addPointer(ffi.Pointer pointer) {
    _pointers.add(pointer);
  }

  void addFunction(ffi.NativeCallable function) {
    _nativeFunctions.add(function);
  }

  void free() {
    for (var pointer in _pointers) {
      malloc.free(pointer);
    }
    for (var function in _nativeFunctions) {
      function.close();
    }
  }

  ffi.Pointer<T> allocate<T extends ffi.NativeType>(int size) {
    var pointer = malloc.allocate<T>(size);
    addPointer(pointer);
    return pointer;
  }
}

extension Utf8String on String {
  ffi.Pointer<ffi.Char> toNative([NativeFreeable? freeable]) {
    var p = toNativeUtf8().cast();
    freeable?.addPointer(p);
    return p.cast();
  }
}