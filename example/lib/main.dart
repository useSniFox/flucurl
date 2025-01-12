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

  String method = 'GET';

  String headers = "User-Agent: Flucurl\n";

  String body = '';

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Flucurl Example'),
        ),
        body: Padding(
          padding: EdgeInsets.all(8),
          child: SingleChildScrollView(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(children: [
                  Container(
                    decoration: BoxDecoration(
                      border:
                          Border.all(color: ColorScheme.of(context).outline),
                      borderRadius: BorderRadius.circular(4),
                    ),
                    child: DropdownButton(
                      value: method,
                      items: ['GET', 'POST', 'PUT', 'DELETE', 'PATCH']
                          .map((e) => DropdownMenuItem(
                                value: e,
                                child: Padding(
                                  padding: const EdgeInsets.all(8.0),
                                  child: Text(e),
                                ),
                              ))
                          .toList(),
                      onChanged: (value) {
                        setState(() {
                          method = value ?? 'GET';
                        });
                      },
                      underline: Container(),
                    ),
                  ),
                  const SizedBox(width: 16),
                  Expanded(
                    child: TextField(
                      decoration: const InputDecoration(
                        labelText: 'URL',
                        border: OutlineInputBorder(),
                      ),
                      onChanged: (value) {
                        url = value;
                      },
                      controller: TextEditingController(text: url),
                    ),
                  )
                ]),
                const SizedBox(height: 16),
                Align(
                  alignment: Alignment.centerRight,
                  child: FilledButton(
                    onPressed: testFlucurl,
                    child: loading
                        ? SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(
                              color: Colors.white,
                            ),
                          )
                        : Text('Send Request'),
                  ),
                ),
                const SizedBox(height: 16),
                SizedBox(
                  height: 200,
                  child: Row(
                    children: [
                      Expanded(
                        child: TextField(
                          expands: true,
                          maxLines: null,
                          decoration: const InputDecoration(
                            hintText: 'Headers',
                            border: OutlineInputBorder(),
                          ),
                          onChanged: (value) {
                            headers = value;
                          },
                          controller: TextEditingController(text: headers),
                          textAlignVertical: TextAlignVertical.top,
                        ),
                      ),
                      const SizedBox(width: 16),
                      Expanded(
                        child: TextField(
                          expands: true,
                          maxLines: null,
                          decoration: const InputDecoration(
                            hintText: 'Body',
                            border: OutlineInputBorder(),
                          ),
                          onChanged: (value) {
                            body = value;
                          },
                          controller: TextEditingController(text: body),
                          textAlignVertical: TextAlignVertical.top,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                Padding(
                  padding: const EdgeInsets.all(6.0),
                  child: Text(
                    'Response:',
                    style: TextStyle(fontSize: 16),
                  ),
                ),
                Container(
                  width: double.infinity,
                  height: 400,
                  decoration: BoxDecoration(
                    border: Border.all(color: Colors.grey),
                    borderRadius: BorderRadius.circular(4),
                  ),
                  child: SingleChildScrollView(
                    child: buildResponse(),
                  ),
                )
              ],
            ),
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
      ),
    );
  }

  void testFlucurl() async {
    if (loading) return;
    try {
      setState(() {
        loading = true;
      });
      var dio = FlucurlDio(
        baseOptions: BaseOptions(validateStatus: (i) => true),
      );
      var res = await dio.request(
        url,
        data: body.isEmpty ? null : body,
        options: Options(
          method: method,
          headers: Map.fromEntries(
              headers.split('\n').where((e) => e.trim().isNotEmpty).map((e) {
            var parts = e.split(':');
            return MapEntry(parts[0].trim(), parts[1].trim());
          })),
        ),
      );
      setState(() {
        response = res.data.toString();
      });
    } catch (e, s) {
      setState(() {
        response = "Error: $e";
      });
      print(e);
      print(s);
    }
    finally {
      setState(() {
        loading = false;
      });
    }
  }
}
