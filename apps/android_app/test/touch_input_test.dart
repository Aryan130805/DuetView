import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/models/pc_device.dart';
import 'package:second_screen_android/services/connection_service.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';
import 'package:second_screen_android/widgets/touch_input_overlay.dart';
import 'package:second_screen_android/widgets/on_screen_keyboard_bar.dart';

class MockConnectionService extends ChangeNotifier implements ConnectionService {
  final ConnectionStateModel _state = const ConnectionStateModel(
    status: ConnectionStatus.streaming,
    activeProtocol: StreamingProtocol.ss02H264,
  );
  final ValueNotifier<Uint8List?> _latestFrame = ValueNotifier<Uint8List?>(null);

  final List<Map<String, dynamic>> recordedPointerEvents = [];
  final List<Map<String, dynamic>> recordedScrollEvents = [];
  final List<Map<String, dynamic>> recordedKeyEvents = [];

  @override
  ConnectionStateModel get state => _state;

  @override
  ValueListenable<Uint8List?> get latestFrame => _latestFrame;

  @override
  SecondScreenDecoderBridge? get decoderBridge => null;

  @override
  PcDevice? get lastConnectedDevice => null;

  @override
  bool get isSocketConnected => true;

  @override
  bool get isReconnecting => false;

  @override
  Future<bool> reconnect({
    int maxAttempts = 5,
    Duration initialDelay = const Duration(milliseconds: 500),
  }) async => true;

  @override
  Future<bool> connectToDevice(String address, int port, {String? pcName, bool isUsb = false}) async => true;

  @override
  Future<void> disconnect({String? reason}) async {}

  @override
  void cancelConnection() {}

  @override
  Future<bool> sendKeyframeRequest() async => true;

  @override
  Future<bool> sendReceiverReport({int? receivedFps, int? decodedFps, int? droppedFrames, int? bufferDelayMs}) async => true;

  @override
  Future<bool> sendPointerEvent({
    required PointerEventType type,
    required double normalizedX,
    required double normalizedY,
    PointerButton button = PointerButton.none,
    int modifiers = 0,
    int flags = 0,
  }) async {
    recordedPointerEvents.add({
      'type': type,
      'normX': normalizedX,
      'normY': normalizedY,
      'button': button,
      'modifiers': modifiers,
    });
    return true;
  }

  @override
  Future<bool> sendScrollEvent({
    required double normalizedX,
    required double normalizedY,
    int deltaX = 0,
    required int deltaY,
  }) async {
    recordedScrollEvents.add({
      'normX': normalizedX,
      'normY': normalizedY,
      'deltaX': deltaX,
      'deltaY': deltaY,
    });
    return true;
  }

  @override
  Future<bool> sendKeyboardEvent({
    required KeyEventType type,
    int keyCode = 0,
    int unicode = 0,
    int modifiers = 0,
  }) async {
    recordedKeyEvents.add({
      'type': type,
      'keyCode': keyCode,
      'unicode': unicode,
      'modifiers': modifiers,
    });
    return true;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('Phase 3 SS02 Input Packet Serialization Tests', () {
    test('createPointerEventPacket serializes 40-byte valid SS02 packet', () {
      final pkt = createPointerEventPacket(
        sequenceNumber: 7,
        timestampUs: 12345678,
        type: PointerEventType.move,
        button: PointerButton.none,
        modifiers: ModifierFlags.ctrl,
        normalizedX: 0.25,
        normalizedY: 0.75,
      );

      expect(pkt.length, equals(40));
      final bd = ByteData.sublistView(pkt);

      // Check header
      expect(pkt[0], equals(0x53)); // 'S'
      expect(pkt[1], equals(0x53)); // 'S'
      expect(pkt[2], equals(0x30)); // '0'
      expect(pkt[3], equals(0x32)); // '2'
      expect(bd.getUint16(4, Endian.big), equals(2)); // version 2
      expect(bd.getUint16(6, Endian.big), equals(Ss02PacketType.pointerEvent.value)); // 0x000B
      expect(bd.getUint32(8, Endian.big), equals(7)); // sequence
      expect(bd.getUint32(12, Endian.big), equals(16)); // 16-byte payload
      expect(bd.getUint64(16, Endian.big), equals(12345678));

      // Check payload
      expect(bd.getUint16(24, Endian.big), equals(PointerEventType.move.value));
      expect(bd.getUint8(26), equals(PointerButton.none.value));
      expect(bd.getUint8(27), equals(ModifierFlags.ctrl));
      expect(bd.getFloat32(28, Endian.big), closeTo(0.25, 0.0001));
      expect(bd.getFloat32(32, Endian.big), closeTo(0.75, 0.0001));
    });

    test('createPointerEventPacket clamps out-of-bounds coordinates', () {
      final pkt = createPointerEventPacket(
        sequenceNumber: 1,
        timestampUs: 100,
        type: PointerEventType.move,
        normalizedX: -0.5,
        normalizedY: 1.5,
      );

      final bd = ByteData.sublistView(pkt);
      expect(bd.getFloat32(28, Endian.big), equals(0.0));
      expect(bd.getFloat32(32, Endian.big), equals(1.0));
    });

    test('createScrollEventPacket serializes 40-byte valid SS02 message', () {
      final pkt = createScrollEventPacket(
        sequenceNumber: 8,
        timestampUs: 99999,
        normalizedX: 0.5,
        normalizedY: 0.5,
        deltaX: -120,
        deltaY: 240,
      );

      expect(pkt.length, equals(40));
      final bd = ByteData.sublistView(pkt);
      expect(bd.getUint16(6, Endian.big), equals(Ss02PacketType.scrollEvent.value)); // 0x000D
      expect(bd.getUint32(12, Endian.big), equals(16));
      expect(bd.getFloat32(24, Endian.big), closeTo(0.5, 0.0001));
      expect(bd.getInt32(32, Endian.big), equals(-120));
      expect(bd.getInt32(36, Endian.big), equals(240));
    });

    test('createKeyboardEventPacket serializes 36-byte valid SS02 message', () {
      final pkt = createKeyboardEventPacket(
        sequenceNumber: 9,
        timestampUs: 100000,
        type: KeyEventType.unicodeChar,
        modifiers: ModifierFlags.shift,
        unicodeCodepoint: 0x20AC, // Euro symbol €
      );

      expect(pkt.length, equals(36));
      final bd = ByteData.sublistView(pkt);
      expect(bd.getUint16(6, Endian.big), equals(Ss02PacketType.keyboardEvent.value)); // 0x000C
      expect(bd.getUint32(12, Endian.big), equals(12));
      expect(bd.getUint16(24, Endian.big), equals(KeyEventType.unicodeChar.value));
      expect(bd.getUint8(26), equals(ModifierFlags.shift));
      expect(bd.getUint32(32, Endian.big), equals(0x20AC));
    });
  });

  group('TouchInputOverlay Widget & Coordinate Mapping Tests', () {
    testWidgets('accurately converts centered viewport touch coordinates to normalized [0.0, 1.0]', (tester) async {
      final mockService = MockConnectionService();

      // Desktop aspect ratio 16:9 = 1.7778
      // Container size 800 x 600 -> container AR = 1.333 (taller than desktop, so letterboxed top and bottom)
      // Rendered desktop width = 800.
      // Rendered desktop height = 800 / (16/9) = 450.
      // Offset Y = (600 - 450) / 2 = 75. Offset X = 0.

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Center(
              child: SizedBox(
                width: 800,
                height: 600,
                child: TouchInputOverlay(
                  connectionService: mockService,
                  desktopAspectRatio: 16 / 9,
                  inputMode: TouchInputMode.direct,
                  child: Container(color: Colors.blue),
                ),
              ),
            ),
          ),
        ),
      );

      // Tap exact center of the rendered screen (X = 400, Y = 300)
      final centerPoint = tester.getCenter(find.byType(TouchInputOverlay));
      await tester.tapAt(centerPoint);
      await tester.pumpAndSettle();

      expect(mockService.recordedPointerEvents.isNotEmpty, isTrue);
      final firstMove = mockService.recordedPointerEvents.firstWhere((e) => e['type'] == PointerEventType.move);
      expect(firstMove['normX'], closeTo(0.5, 0.01));
      expect(firstMove['normY'], closeTo(0.5, 0.01));
    });

    testWidgets('ignores touches occurring in the unused letterbox margins', (tester) async {
      final mockService = MockConnectionService();

      // Container 800 x 600, rendered desktop is between Y = 75 and Y = 525.
      // Top margin is Y = 0..74.
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Center(
              child: SizedBox(
                width: 800,
                height: 600,
                child: TouchInputOverlay(
                  connectionService: mockService,
                  desktopAspectRatio: 16 / 9,
                  inputMode: TouchInputMode.direct,
                  child: Container(color: Colors.blue),
                ),
              ),
            ),
          ),
        ),
      );

      final topLeft = tester.getTopLeft(find.byType(TouchInputOverlay));
      // Tap in top letterbox margin (Y = 20)
      await tester.tapAt(Offset(topLeft.dx + 400, topLeft.dy + 20));
      await tester.pumpAndSettle();

      expect(mockService.recordedPointerEvents.isEmpty, isTrue);
    });

    testWidgets('OnScreenKeyboardBar toggles modifiers and sends unicode keys', (tester) async {
      final mockService = MockConnectionService();
      bool closed = false;

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: OnScreenKeyboardBar(
              connectionService: mockService,
              onClose: () => closed = true,
            ),
          ),
        ),
      );

      // Verify Ctrl button toggle
      expect(find.text('Ctrl'), findsOneWidget);
      await tester.tap(find.text('Ctrl'));
      await tester.pumpAndSettle();

      // Tap Enter key
      expect(find.text('Enter'), findsOneWidget);
      await tester.tap(find.text('Enter'));
      await tester.pumpAndSettle();

      expect(mockService.recordedKeyEvents.length, equals(2)); // keyDown + keyUp
      expect(mockService.recordedKeyEvents[0]['type'], equals(KeyEventType.keyDown));
      expect(mockService.recordedKeyEvents[0]['keyCode'], equals(0x0D));
      expect(mockService.recordedKeyEvents[0]['modifiers'], equals(ModifierFlags.ctrl));

      // Close button
      expect(find.byIcon(Icons.close), findsOneWidget);
      await tester.tap(find.byIcon(Icons.close));
      expect(closed, isTrue);
    });

    testWidgets('OnScreenKeyboardBar shortcut button sends chord sequence', (tester) async {
      final mockService = MockConnectionService();

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: OnScreenKeyboardBar(
              connectionService: mockService,
              onClose: () {},
            ),
          ),
        ),
      );

      // Verify Ctrl+C shortcut button
      expect(find.text('Ctrl+C'), findsOneWidget);
      await tester.ensureVisible(find.text('Ctrl+C'));
      await tester.tap(find.text('Ctrl+C'));
      await tester.pumpAndSettle();

      // Expect 4 events: Ctrl down, 'C' down, 'C' up, Ctrl up
      expect(mockService.recordedKeyEvents.length, equals(4));
      expect(mockService.recordedKeyEvents[0]['type'], equals(KeyEventType.keyDown));
      expect(mockService.recordedKeyEvents[0]['keyCode'], equals(0x11)); // VK_CONTROL
      expect(mockService.recordedKeyEvents[1]['type'], equals(KeyEventType.keyDown));
      expect(mockService.recordedKeyEvents[1]['keyCode'], equals(0x43)); // 'C'
      expect(mockService.recordedKeyEvents[2]['type'], equals(KeyEventType.keyUp));
      expect(mockService.recordedKeyEvents[2]['keyCode'], equals(0x43));
      expect(mockService.recordedKeyEvents[3]['type'], equals(KeyEventType.keyUp));
      expect(mockService.recordedKeyEvents[3]['keyCode'], equals(0x11));
    });

    testWidgets('TouchInputOverlay sends buttonUp on tap release (no false drag)', (tester) async {
      final mockService = MockConnectionService();

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Center(
              child: SizedBox(
                width: 800,
                height: 600,
                child: TouchInputOverlay(
                  connectionService: mockService,
                  desktopAspectRatio: 16 / 9,
                  inputMode: TouchInputMode.direct,
                  child: Container(color: Colors.blue),
                ),
              ),
            ),
          ),
        ),
      );

      final centerPoint = tester.getCenter(find.byType(TouchInputOverlay));
      // Tap and release
      await tester.tapAt(centerPoint);
      await tester.pumpAndSettle();

      // Verify buttonDown was followed by buttonUp (clean click)
      final btnDown = mockService.recordedPointerEvents.where((e) => e['type'] == PointerEventType.buttonDown);
      final btnUp = mockService.recordedPointerEvents.where((e) => e['type'] == PointerEventType.buttonUp);
      expect(btnDown.isNotEmpty, isTrue);
      expect(btnUp.isNotEmpty, isTrue);
    });
  });
}
