import 'dart:async';

import 'package:flutter/material.dart';
import 'package:sphere_uniform_geocoding/sphere_uniform_geocoding.dart'
    as sphere_uniform_geocoding;
import 'package:sphere_uniform_geocoding/sphere_uniform_geocoding_bindings_generated.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  late int sumResult;
  late int calculateSegmentIndexFromLatLngResult;
  late int convertToSegmentIndex2Result;
  late SegGroupAndLocalSegIndex splitSegIndexToSegGroupAndLocalSegmentIndexResult;
  late List<int> getNeighborsOfSegmentIndexResult;
  late List<(double, double)> calculateSegmentCornersInLatLngResult;
  late Future<int> sumAsyncResult;

  @override
  void initState() {
    super.initState();
    sumResult = sphere_uniform_geocoding.sum(1, 2);

    calculateSegmentIndexFromLatLngResult =
        sphere_uniform_geocoding.calculateSegmentIndexFromLatLng(4, 0, 0);
    getNeighborsOfSegmentIndexResult = sphere_uniform_geocoding.getNeighborsOfSegmentIndex(4, 0);

    const int subdivisionCount = 14654;
    convertToSegmentIndex2Result = sphere_uniform_geocoding.convertToSegmentIndex2(subdivisionCount, 19, subdivisionCount*subdivisionCount-1);
    splitSegIndexToSegGroupAndLocalSegmentIndexResult = sphere_uniform_geocoding.splitSegIndexToSegGroupAndLocalSegmentIndex(subdivisionCount, convertToSegmentIndex2Result);

    calculateSegmentCornersInLatLngResult = sphere_uniform_geocoding.calculateSegmentCornersInLatLng(8192, 0);

    sumAsyncResult = sphere_uniform_geocoding.sumAsync(3, 4);


  }

  @override
  Widget build(BuildContext context) {
    const textStyle = TextStyle(fontSize: 25);
    const spacerSmall = SizedBox(height: 10);
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Native Packages'),
        ),
        body: SingleChildScrollView(
          child: Container(
            padding: const EdgeInsets.all(10),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'This calls a native function through FFI that is shipped as source in the package. '
                  'The native code is built as part of the Flutter Runner build.',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                spacerSmall,
                Text(
                  'sum(1, 2) = $sumResult',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                Text(
                  'calculateSegmentIndexFromLatLngResult(4, 0, 0) = $calculateSegmentIndexFromLatLngResult',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                Text(
                  'convertToSegmentIndex2Result = $convertToSegmentIndex2Result',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                Text(
                  'splitSegIndexToSegGroupAndLocalSegmentIndexResult = (segGroup: ${splitSegIndexToSegGroupAndLocalSegmentIndexResult.segGroup}, localSegIndex: ${splitSegIndexToSegGroupAndLocalSegmentIndexResult.localSegIndex})',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                Text(
                  'Segment corners = $calculateSegmentCornersInLatLngResult',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                Text(
                  'getNeighborsOfSegmentIndexResult(4, 0) = $getNeighborsOfSegmentIndexResult',
                  style: textStyle,
                  textAlign: TextAlign.center,
                ),
                for (var i = 0; i < 16; i++) ...{
                  Text(
                    'seg #$i Center: ${sphere_uniform_geocoding.calculateSegmentCenterPos(8192, i).toCustomString()} / LL: ${sphere_uniform_geocoding.calculateSegmentCenter(8192, i)}',
                    style: textStyle,
                    textAlign: TextAlign.center,
                  ),
                },
                spacerSmall,
                FutureBuilder<int>(
                  future: sumAsyncResult,
                  builder: (BuildContext context, AsyncSnapshot<int> value) {
                    final displayValue =
                        (value.hasData) ? value.data : 'loading';
                    return Text(
                      'await sumAsync(3, 4) = $displayValue',
                      style: textStyle,
                      textAlign: TextAlign.center,
                    );
                  },
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
