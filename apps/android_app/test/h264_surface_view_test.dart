import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';
import 'package:second_screen_android/widgets/h264_surface_view.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const channel = MethodChannel('com.secondscreen/decoder_control');

  setUp(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
      switch (methodCall.method) {
        case 'feedAccessUnit':
          return true;
        default:
          return true;
      }
    });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  testWidgets('H264SurfaceView renders loading overlay when awaiting config',
      (WidgetTester tester) async {
    final bridge = SecondScreenDecoderBridge();

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: H264SurfaceView(
            bridge: bridge,
            aspectRatio: 16 / 9,
          ),
        ),
      ),
    );

    // Default state is UNINITIALIZED -> overlay visible
    expect(find.byType(CircularProgressIndicator), findsOneWidget);
    expect(find.text('Preparing Secondary Display...'), findsOneWidget);
    expect(find.textContaining('H.264 MediaCodec: UNINITIALIZED'), findsOneWidget);

    bridge.dispose();
  });

  testWidgets('H264SurfaceView hides loading overlay when DECODING',
      (WidgetTester tester) async {
    final bridge = SecondScreenDecoderBridge();

    // Simulate native platform notifying bridge of state transition to DECODING
    // by sending onStateChanged method call from platform channel
    final byteData = const StandardMethodCodec().encodeMethodCall(
      const MethodCall('onStateChanged', {'state': 'DECODING'}),
    );
    await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .handlePlatformMessage('com.secondscreen/decoder_control', byteData, (data) {});

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: H264SurfaceView(
            bridge: bridge,
            aspectRatio: 16 / 9,
          ),
        ),
      ),
    );

    expect(find.byType(CircularProgressIndicator), findsNothing);
    expect(find.textContaining('H.264 MediaCodec: DECODING'), findsOneWidget);

    bridge.dispose();
  });
}
