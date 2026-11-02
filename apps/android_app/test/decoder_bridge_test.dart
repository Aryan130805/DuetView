import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const channel = MethodChannel('com.secondscreen/decoder_control');
  final List<MethodCall> log = <MethodCall>[];

  setUp(() {
    log.clear();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
      log.add(methodCall);
      switch (methodCall.method) {
        case 'getDecoderCapabilities':
          return {
            'codecName': 'c2.qti.avc.decoder',
            'isHardwareAccelerated': true,
            'maxWidth': 3840,
            'maxHeight': 2160,
            'isSupported': true,
          };
        case 'initializeDecoder':
          return true;
        case 'feedConfigPacket':
          return true;
        case 'feedAccessUnit':
          return true;
        case 'resetDecoder':
          return true;
        case 'getMetrics':
          return {
            'codecName': 'c2.qti.avc.decoder',
            'isHardwareAccelerated': true,
            'state': 'DECODING',
            'configuredWidth': 1280,
            'configuredHeight': 720,
            'submittedFrames': 42,
            'decodedFrames': 40,
            'outputFps': 29.8,
            'decodeErrors': 0,
            'keyframeWaitCount': 1,
          };
        case 'runSmokeTest':
          return {
            'success': true,
            'codecName': 'c2.qti.avc.decoder',
            'isHardwareAccelerated': true,
            'submittedFrames': 15,
            'decodedFrames': 15,
            'decodeErrors': 0,
            'state': 'DECODING',
          };
        case 'setKeepScreenOn':
          return true;
        default:
          return null;
      }
    });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  group('SecondScreenDecoderBridge Tests', () {
    test('getDecoderCapabilities deserializes native codec metadata accurately',
        () async {
      final bridge = SecondScreenDecoderBridge();
      final caps = await bridge.getDecoderCapabilities();

      expect(caps.codecName, equals('c2.qti.avc.decoder'));
      expect(caps.isHardwareAccelerated, isTrue);
      expect(caps.maxWidth, equals(3840));
      expect(caps.maxHeight, equals(2160));
      expect(caps.isSupported, isTrue);
      expect(log.single.method, equals('getDecoderCapabilities'));
      bridge.dispose();
    });

    test('initializeDecoder transmits target dimensions', () async {
      final bridge = SecondScreenDecoderBridge();
      final ok = await bridge.initializeDecoder(width: 1920, height: 1080);

      expect(ok, isTrue);
      expect(log.single.method, equals('initializeDecoder'));
      expect(log.single.arguments, equals({'width': 1920, 'height': 1080}));
      bridge.dispose();
    });

    test('feedConfigPacket passes binary payload correctly', () async {
      final bridge = SecondScreenDecoderBridge();
      final spsPps = Uint8List.fromList([0, 0, 0, 1, 0x67, 0, 0, 0, 1, 0x68]);
      final ok = await bridge.feedConfigPacket(spsPps);

      expect(ok, isTrue);
      expect(log.single.method, equals('feedConfigPacket'));
      expect(log.single.arguments['data'], equals(spsPps));
      bridge.dispose();
    });

    test('feedAccessUnit serializes timestamp and keyframe flag', () async {
      final bridge = SecondScreenDecoderBridge();
      final idr = Uint8List.fromList([0, 0, 0, 1, 0x65, 0x88]);
      final ok = await bridge.feedAccessUnit(idr,
          timestampUs: 1234567, isKeyframe: true);

      expect(ok, isTrue);
      expect(log.single.method, equals('feedAccessUnit'));
      expect(log.single.arguments['timestampUs'], equals(1234567));
      expect(log.single.arguments['isKeyframe'], isTrue);
      bridge.dispose();
    });

    test('getMetrics parses native decoder performance numbers', () async {
      final bridge = SecondScreenDecoderBridge();
      final metrics = await bridge.getMetrics();

      expect(metrics.codecName, equals('c2.qti.avc.decoder'));
      expect(metrics.isHardwareAccelerated, isTrue);
      expect(metrics.state, equals('DECODING'));
      expect(metrics.submittedFrames, equals(42));
      expect(metrics.decodedFrames, equals(40));
      expect(metrics.outputFps, equals(29.8));
      expect(metrics.decodeErrors, equals(0));
      expect(metrics.keyframeWaitCount, equals(1));
      bridge.dispose();
    });

    test('runSmokeTest triggers self-contained decoding check', () async {
      final bridge = SecondScreenDecoderBridge();
      final result = await bridge.runSmokeTest(width: 1280, height: 720, frames: 15);

      expect(result['success'], isTrue);
      expect(result['submittedFrames'], equals(15));
      expect(result['decodedFrames'], equals(15));
      expect(result['state'], equals('DECODING'));
      bridge.dispose();
    });

    test('resetDecoder invokes reset on native bridge', () async {
      final bridge = SecondScreenDecoderBridge();
      final ok = await bridge.resetDecoder();

      expect(ok, isTrue);
      expect(log.single.method, equals('resetDecoder'));
      bridge.dispose();
    });

    test('event streams propagate native state changes, keyframe requests, and errors',
        () async {
      final bridge = SecondScreenDecoderBridge();
      final states = <String>[];
      var keyframeRequested = false;
      final errors = <String>[];

      bridge.onStateChanged.listen(states.add);
      bridge.onKeyframeRequested.listen((_) => keyframeRequested = true);
      bridge.onDecoderError.listen(errors.add);

      // Simulate native platform sending onStateChanged
      final stateMsg = const StandardMethodCodec().encodeMethodCall(
        const MethodCall('onStateChanged', {'state': 'WAITING_FOR_KEYFRAME'}),
      );
      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage('com.secondscreen/decoder_control', stateMsg, (data) {});

      // Simulate native platform sending onKeyframeRequested
      final keyframeMsg = const StandardMethodCodec().encodeMethodCall(
        const MethodCall('onKeyframeRequested', null),
      );
      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage('com.secondscreen/decoder_control', keyframeMsg, (data) {});

      // Simulate native platform sending onDecoderError
      final errorMsg = const StandardMethodCodec().encodeMethodCall(
        const MethodCall('onDecoderError', {'error': 'Surface destroyed', 'isFatal': false}),
      );
      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage('com.secondscreen/decoder_control', errorMsg, (data) {});

      expect(states, equals(['WAITING_FOR_KEYFRAME']));
      expect(bridge.currentState, equals('WAITING_FOR_KEYFRAME'));
      expect(keyframeRequested, isTrue);
      expect(errors, equals(['Surface destroyed']));

      bridge.dispose();
    });

    test('setKeepScreenOn invokes native method channel with enable argument', () async {
      final bridge = SecondScreenDecoderBridge();
      final res = await bridge.setKeepScreenOn(true);
      expect(res, isTrue);
      expect(log.single.method, equals('setKeepScreenOn'));
      expect(log.single.arguments, equals({'enable': true}));

      final clearRes = await bridge.setKeepScreenOn(false);
      expect(clearRes, isTrue);
      expect(log.last.method, equals('setKeepScreenOn'));
      expect(log.last.arguments, equals({'enable': false}));

      bridge.dispose();
    });
  });
}
