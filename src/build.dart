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
  var result = Process.runSync('cmake', ["--preset=default"]);
  if (result.exitCode != 0) {
    print(result.stderr);
    exit(result.exitCode);
  }
  result = Process.runSync('cmake', ["--build", "build"]);
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