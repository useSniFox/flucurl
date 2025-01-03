import 'dart:io';

late String platform;

void main(List<String> args) {
  if (args.isEmpty) {
    print('Usage: dart build.dart <platform>');
    exit(1);
  }
  platform = args[0];
  Directory.current = 'src';
  findVcpkg();
  var compiler = args[1];
  var generator = args[2];
  var cmakeRoot = args[3];
  print(["--preset=default", "-DCMAKE_CXX_COMPILER=$compiler", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_PROGRAMS=OFF", "-G", generator]);
  var result = Process.runSync(cmakeRoot, ["--preset=default", "-DCMAKE_CXX_COMPILER=$compiler", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_PROGRAMS=OFF", "-G", generator]);
  if (result.exitCode != 0) {
    print(result.stderr);
    exit(result.exitCode);
  }
  result = Process.runSync(cmakeRoot, ["--build", "build", "--config Release"]);
  if (result.exitCode != 0) {
    print(result.stderr);
    exit(result.exitCode);
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
