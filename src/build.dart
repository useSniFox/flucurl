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
  Process.runSync('cmake', ["--preset=default"]);
  Process.runSync('cmake', ["--build", "build"]);
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
  var content = '''
{
    "version": 2,
    "configurePresets": [
        {
            "name": "default",
            "inherits": "vcpkg",
            "environment": {
                "VCPKG_ROOT": $vcpkgRoot
            }
        }
    ]
}
''';
  File('CMakeUserPresets.json').writeAsStringSync(content);
}