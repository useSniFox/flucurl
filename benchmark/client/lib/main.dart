import 'package:dio/dio.dart';
import 'package:flucurl/flucurl_dio.dart';
import 'package:flutter/material.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
      ),
      home: Scaffold(
          appBar: AppBar(
            title: const Text('Native Packages'),
          ),
          body: Center(
            child: Column(
              children: [
                FilledButton(
                  onPressed: testFlucurl,
                  child: Text("Test Flucurl"),
                ),
                const SizedBox(height: 20),
                FilledButton(
                  onPressed: testDio,
                  child: Text("Test Dio"),
                ),
              ],
            ),
          )),
    );
  }

  void testFlucurl() async {
    print("Testing Flucurl");
    var dio = FlucurlDio(
      baseOptions: BaseOptions(validateStatus: (i) => true),
    );
    await testBase(dio);
    dio.close();
  }

  void testDio() async {
    print("Testing Dio");
    var dio = Dio(BaseOptions(validateStatus: (i) => true));
    await testBase(dio);
    dio.close();
  }

  Future<void> testBase(Dio dio) async {
    var url = "http://localhost:8080";
    var stopwatch = Stopwatch()..start();
    for (var i = 0; i < 1000; i++) {
      await dio.get("$url/size/1");
    }
    print("Small Files Time: ${stopwatch.elapsedMilliseconds}");
    stopwatch.stop();
    stopwatch.reset();
    stopwatch.start();
    for (var i = 0; i < 100; i++) {
      await dio.get("$url/size/10000");
    }
    print("Large Files Time: ${stopwatch.elapsedMilliseconds}");
    stopwatch.stop();
  }
}
