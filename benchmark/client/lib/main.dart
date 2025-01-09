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
  var result = '';

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
                Text(
                  "Single Threaded Benchmark",
                  style: TextStyle(fontSize: 20),
                ),
                const SizedBox(height: 8),
                FilledButton(
                  onPressed: testFlucurl,
                  child: Text("Test Flucurl"),
                ),
                const SizedBox(height: 20),
                FilledButton(
                  onPressed: testDio,
                  child: Text("Test Dio"),
                ),
                const SizedBox(height: 20),
                Text(
                  "Multi Threaded Benchmark",
                  style: TextStyle(fontSize: 20),
                ),
                const SizedBox(height: 8),
                FilledButton(
                  onPressed: testFlucurlMultiThreaded,
                  child: Text("Test Flucurl"),
                ),
                const SizedBox(height: 20),
                FilledButton(
                  onPressed: testDioMultiThreaded,
                  child: Text("Test Dio"),
                ),
                const SizedBox(height: 20),
                Text(result),
              ],
            ),
          )),
    );
  }

  void testFlucurl() async {
    setState(() {
      result = "Testing Flucurl";
    });
    var dio = FlucurlDio(
      baseOptions: BaseOptions(validateStatus: (i) => true),
    );
    await testBase(dio);
    dio.close();
  }

  void testDio() async {
    setState(() {
      result = "Testing Dio";
    });
    var dio = Dio(BaseOptions(validateStatus: (i) => true));
    await testBase(dio);
    dio.close();
  }

  Future<void> testBase(Dio dio) async {
    var url = "http://localhost:8080";
    var stopwatch = Stopwatch()..start();
    for (var i = 0; i < 10000; i++) {
      await dio.get("$url/size/1");
    }
    setState(() {
      result += "\nSmall Files Time: ${stopwatch.elapsedMilliseconds}";
    });
    stopwatch.stop();
    stopwatch.reset();
    stopwatch.start();
    for (var i = 0; i < 100; i++) {
      await dio.get("$url/size/10000");
    }
    setState(() {
      result += "\nBig Files Time: ${stopwatch.elapsedMilliseconds}";
    });
    stopwatch.stop();
  }

  Future<void> multiThreadedBase(Dio dio) async {
    var url = "http://localhost:8080";
    var stopwatch = Stopwatch()..start();
    var futures = <Future>[];
    for (var i = 0; i < 10000; i++) {
      futures.add(dio.get("$url/size/1"));
    }
    await Future.wait(futures);
    setState(() {
      result += "\nSmall Files Time: ${stopwatch.elapsedMilliseconds}";
    });
    stopwatch.stop();
    stopwatch.reset();
    stopwatch.start();
    futures = <Future>[];
    for (var i = 0; i < 100; i++) {
      futures.add(dio.get("$url/size/10000"));
    }
    await Future.wait(futures);
    setState(() {
      result += "\nBig Files Time: ${stopwatch.elapsedMilliseconds}";
    });
    stopwatch.stop();
  }

  Future<void> testFlucurlMultiThreaded() async {
    setState(() {
      result = "Testing Flucurl";
    });
    var dio = FlucurlDio(
      baseOptions: BaseOptions(validateStatus: (i) => true),
    );
    await multiThreadedBase(dio);
    dio.close();
  }

  Future<void> testDioMultiThreaded() async {
    setState(() {
      result = "Testing Dio";
    });
    var dio = Dio(BaseOptions(validateStatus: (i) => true));
    await multiThreadedBase(dio);
    dio.close();
  }
}
