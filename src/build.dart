import 'dart:io';

late String platform;

void main(List<String> args) {
  if (args.isEmpty) {
    print('Usage: dart build.dart <platform> <compiler> <generator> <cmake>');
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
    // TODO
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
