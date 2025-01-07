import 'package:dio/dio.dart';
import 'package:flucurl/flucurl_dio.dart';
import 'package:flutter/material.dart';

void main() {
  Flucurl.init();
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  var url = 'http://example.com';

  String response = '';

  bool loading = false;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Native Packages'),
        ),
        body: Padding(
          padding: EdgeInsets.all(8),
          child: Column(
            children: [
              TextField(
                decoration: const InputDecoration(
                  labelText: 'URL',
                  border: OutlineInputBorder(),
                ),
                onChanged: (value) {
                  url = value;
                },
                controller: TextEditingController(text: url),
              ),
              const SizedBox(height: 16),
              Align(
                alignment: Alignment.centerRight,
                child: FilledButton(
                  onPressed: () async {
                    if (url.isEmpty || loading) {
                      return;
                    }
                    var dio = FlucurlDio(
                          baseOptions:
                              BaseOptions(validateStatus: (i) => true));
                    try {
                      response = '';
                      setState(() {
                        loading = true;
                      });
                      var res = await dio.get(url);
                      response = "Status Code: ${res.statusCode}\n";
                      response += "Headers:\n";
                      for (var entry in res.headers.map.entries) {
                        response += "${entry.key}: ${entry.value}\n";
                      }
                      response += "Body:\n";
                      response += res.data.toString();
                    } catch (e) {
                      print(e);
                    } finally {
                      setState(() {
                        loading = false;
                      });
                      dio.close();
                    }
                  },
                  child: loading
                      ? SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(color: Colors.white,),
                        )
                      : Text('Send Request'),
                ),
              ),
              const SizedBox(height: 16),
              Expanded(
                child: Container(
                  width: double.infinity,
                  decoration: BoxDecoration(
                    border: Border.all(color: Colors.grey),
                  ),
                  child: SingleChildScrollView(
                    child: buildResponse(),
                  ),
                ),
              )
            ],
          ),
        ),
      ),
    );
  }

  Widget buildResponse() {
    return SizedBox(
        width: double.infinity,
        child: Text(
          response,
          style: const TextStyle(fontSize: 16),
        )
      );
  }
}
