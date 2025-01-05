import 'dart:convert';
import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';
import 'package:flucurl/src/flucurl_bindings_generated.dart' as bindings;

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
      var freed = false;
      for (var pool in _memPolls) {
        if (freed = pool.maybeFree(pointer)) {
          break;
        }
      }
      if (!freed) {
        malloc.free(pointer);
      }
    }
    for (var function in _nativeFunctions) {
      function.close();
    }
  }

  ffi.Pointer<T> allocate<T extends ffi.NativeType>(int size) {
    for (var pool in _fixedSizeMemPools) {
      if (size == pool.itemSize) {
        var p = pool.allocate();
        addPointer(p);
        return p.cast();
      }
    }
    for (var pool in _dynamicSizeMemPools) {
      if (size <= pool.itemSize) {
        var p = pool.allocate();
        addPointer(p);
        return p.cast();
      }
    }
    var p = malloc.allocate(size);
    addPointer(p);
    return p.cast();
  }

  static final _fixedSizeMemPools = [
    // bindings.Config
    _MemPool(10, ffi.sizeOf<bindings.Config>()),
    // bindings.TLSConfig
    _MemPool(10, ffi.sizeOf<bindings.TLSConfig>()),
    // bindings.Request
    _MemPool(20, ffi.sizeOf<bindings.Request>()),
  ];

  static final _dynamicSizeMemPools = [
    // small
    _MemPool(500, 128),
    // medium
    _MemPool(100, 512), 
    // large
    _MemPool(20, 2048),
  ];

  List<_MemPool> get _memPolls => [..._fixedSizeMemPools, ..._dynamicSizeMemPools];
}

extension Utf8String on String {
  ffi.Pointer<ffi.Char> toNative(NativeFreeable freeable) {
    final units = utf8.encode(this);
    final result = freeable.allocate<ffi.Uint8>(units.length + 1);
    final nativeString = result.asTypedList(units.length + 1);
    nativeString.setAll(0, units);
    nativeString[units.length] = 0;
    return result.cast();
  }
}

class _MemPool {
  final int length;

  final int itemSize;

  final _mem = <ffi.Pointer, bool>{};

  _MemPool(this.length, this.itemSize) {
    for (var i = 0; i < length; i++) {
      var p = malloc.allocate(itemSize);
      _mem[p] = false;
    }
  }

  ffi.Pointer allocate() {
    for (var p in _mem.keys) {
      if (!_mem[p]!) {
        _mem[p] = true;
        return p;
      }
    }
    return malloc.allocate(itemSize);
  }

  void free(ffi.Pointer pointer) {
    if (_mem.containsKey(pointer)) {
      _mem[pointer] = false;
    } else {
      malloc.free(pointer);
    }
  }

  bool maybeFree(ffi.Pointer pointer) {
    if (_mem.containsKey(pointer)) {
      _mem[pointer] = false;
      return true;
    }
    return false;
  }
}