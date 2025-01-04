import 'dart:io';

import 'package:dio/dio.dart';
import 'package:crypto/crypto.dart';
late String platform;

const curlVersion = '8.11.1';

void main(List<String> args) async {
  if (args.isEmpty) {
    stdout.writeln('Usage: dart build.dart <platform> <compiler> <generator> <cmake>');
    exit(1);
  }
  platform = args[0];
  Directory.current = 'src';
  var compiler = args[1];
  var generator = args[2];
  var cmakeRoot = args[3];
  var buildDir = Directory('build');
  if (buildDir.existsSync()) {
    buildDir.deleteSync(recursive: true);
  }
  if (platform == "windows") {
    findVcpkg();
    var result = Process.runSync(cmakeRoot, ["--preset=default", "-DCMAKE_CXX_COMPILER=$compiler", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_PROGRAMS=OFF", "-G", generator]);
    stdout.writeln(result.stdout);
    if (result.exitCode != 0) {
      stderr.writeln(result.stderr);
      exit(result.exitCode);
    }
    result = Process.runSync(cmakeRoot, ["--build", "build", "--config Release"]);
    stdout.writeln(result.stdout);
    if (result.exitCode != 0) {
      stderr.writeln(result.stderr);
      exit(result.exitCode);
    }
  } else if (platform == "linux") {
    var curlDir = Directory('curl-x86_64');
    if (curlDir.existsSync()) {
      curlDir.deleteSync(recursive: true);
    }
    var curlFile = File('curl.tar.xz');
    if (curlFile.existsSync()) {
      const sha256Result = "10CB7624D74FEE6F927B6B67850D63A93B6E8D1E15BB0D400137B61EFAE815D4";
      final bytes = await curlFile.readAsBytes();
      final hash = sha256.convert(bytes).toString().toUpperCase();
      if (hash != sha256Result) {
        curlFile.deleteSync();
      } else {
        stdout.writeln('curl $curlVersion already downloaded');
      }
    }
    if (!curlFile.existsSync()) {
      stdout.writeln('Downloading curl $curlVersion...');
      final url = "https://github.com/useSniFox/static-curl/releases/download/v$curlVersion/curl-linux-x86_64-dev-$curlVersion.tar.xz";
      await Dio().download(url, 'curl.tar.xz');
    }
    Process.runSync('tar', ['-xf', 'curl.tar.xz']);
    buildDir.createSync();
    Directory.current = 'build';
    var result = Process.runSync(cmakeRoot, ["-DCMAKE_CXX_COMPILER=$compiler", "-DCMAKE_BUILD_TYPE=Release", "-G", generator, ".."]);
    stdout.writeln(result.stdout);
    if (result.exitCode != 0) {
      stderr.writeln(result.stderr);
      exit(result.exitCode);
    }
    result = Process.runSync(cmakeRoot, ["--build", ".", "--config Release"]);
    stdout.writeln(result.stdout);
    if (result.exitCode != 0) {
      stderr.writeln(result.stderr);
      exit(result.exitCode);
    }
  } else {
    throw 'Unsupported platform: $platform';
  }
}

void findVcpkg() {
  var vcpkgRoot = Platform.environment['VCPKG_ROOT'];
  if (vcpkgRoot == null) {
    var paths = Platform.environment['PATH']!.split(platform == 'windows' ? ';' : ':');
    for (var path in paths) {
      var vcpkgPath = '$path${Platform.pathSeparator}.vcpkg-root';
      if (File(vcpkgPath).existsSync()) {
        vcpkgRoot = path;
        break;
      }
    }
  }
  if (vcpkgRoot == null) {
    throw 'VCPKG_ROOT not found';
  }
  vcpkgRoot = vcpkgRoot.replaceAll(Platform.pathSeparator, '/');
  var content = '''
{
    "version": 2,
    "configurePresets": [
        {
            "name": "default",
            "inherits": "vcpkg",
            "environment": {
                "VCPKG_ROOT": "$vcpkgRoot"
            }
        }
    ]
}
''';
  File('CMakeUserPresets.json').writeAsStringSync(content);
}
