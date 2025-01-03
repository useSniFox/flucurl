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
  var url = '';

  Response? response;

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
                  setState(() {
                    url = value;
                  });
                },
              ),
              const SizedBox(height: 16),
              Align(
                alignment: Alignment.centerRight,
                child: FilledButton(
                  onPressed: () async {
                    if (url.isEmpty || loading) {
                      return;
                    }
                    setState(() {
                      loading = true;
                    });
                    var dio = FlucurlDio();
                    response = await dio.get(url);
                    setState(() {
                      loading = false;
                    });
                  },
                  child: loading
                      ? SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(),
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
    if (response == null) {
      return const SizedBox();
    } else {
      return SizedBox(
        width: double.infinity,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text("Status Code: ${response!.statusCode}"),
            Text("Headers:"),
            for (var entry in response!.headers.map.entries)
              Text("${entry.key}: ${entry.value}"),
            Text("Body:"),
            Text(response!.data.toString()),
          ],
        ),
      );
    }
  }
}
