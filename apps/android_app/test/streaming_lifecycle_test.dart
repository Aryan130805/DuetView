import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/models/pc_device.dart';
import 'package:second_screen_android/models/stream_settings.dart';
import 'package:second_screen_android/screens/streaming_screen.dart';
import 'package:second_screen_android/services/connection_service.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';
import 'package:second_screen_android/services/settings_service.dart';

class LifecycleMockConnectionService extends ChangeNotifier
    implements ConnectionService {
  ConnectionStateModel _state = const ConnectionStateModel(
    status: ConnectionStatus.streaming,
    activeProtocol: StreamingProtocol.ss02H264,
  );
  final ValueNotifier<Uint8List?> _latestFrame = ValueNotifier<Uint8List?>(
    null,
  );

  bool mockSocketConnected = true;
  bool mockReconnecting = false;
  PcDevice? mockLastDevice = const PcDevice(
    id: '192.168.1.100:52025',
    name: 'Windows Host PC',
    address: '192.168.1.100',
    port: 52025,
  );

  int keyframeRequestCount = 0;
  int reconnectCallCount = 0;

  @override
  ConnectionStateModel get state => _state;

  @override
  ValueListenable<Uint8List?> get latestFrame => _latestFrame;

  @override
  SecondScreenDecoderBridge? get decoderBridge => null;

  @override
  PcDevice? get lastConnectedDevice => mockLastDevice;

  @override
  bool get isSocketConnected => mockSocketConnected;

  @override
  bool get isReconnecting => mockReconnecting;

  @override
  Future<bool> sendKeyframeRequest() async {
    keyframeRequestCount++;
    return true;
  }

  @override
  Future<bool> reconnect({
    int maxAttempts = 5,
    Duration initialDelay = const Duration(milliseconds: 500),
  }) async {
    reconnectCallCount++;
    mockSocketConnected = true;
    _state = _state.copyWith(status: ConnectionStatus.streaming);
    notifyListeners();
    return true;
  }

  @override
  Future<bool> connectToDevice(
    String address,
    int port, {
    String? pcName,
    bool isUsb = false,
  }) async {
    mockSocketConnected = true;
    _state = _state.copyWith(status: ConnectionStatus.streaming);
    notifyListeners();
    return true;
  }

  @override
  Future<void> disconnect({String? reason}) async {
    mockSocketConnected = false;
    _state = ConnectionStateModel(
      status: reason != null
          ? ConnectionStatus.failed
          : ConnectionStatus.disconnected,
      errorMessage: reason,
      connectedDevice: reason != null ? mockLastDevice : null,
    );
    notifyListeners();
  }

  @override
  void cancelConnection() {}

  @override
  Future<bool> sendReceiverReport({
    int? receivedFps,
    int? decodedFps,
    int? droppedFrames,
    int? bufferDelayMs,
  }) async => true;

  @override
  Future<bool> sendPointerEvent({
    required PointerEventType type,
    required double normalizedX,
    required double normalizedY,
    PointerButton button = PointerButton.none,
    int modifiers = 0,
    int flags = 0,
  }) async => true;

  @override
  Future<bool> sendScrollEvent({
    required double normalizedX,
    required double normalizedY,
    int deltaX = 0,
    required int deltaY,
  }) async => true;

  @override
  Future<bool> sendKeyboardEvent({
    required KeyEventType type,
    int keyCode = 0,
    int unicode = 0,
    int modifiers = 0,
  }) async => true;
}

class LifecycleMockSettingsService extends ChangeNotifier
    implements SettingsService {
  StreamSettings _settings = const StreamSettings();

  @override
  StreamSettings get settings => _settings;

  @override
  bool get isLoaded => true;

  @override
  Future<void> loadSettings({dynamic customPrefs}) async {}

  @override
  Future<void> updateSettings(
    StreamSettings newSettings, {
    dynamic customPrefs,
  }) async {
    _settings = newSettings;
    notifyListeners();
  }

  @override
  Future<void> saveLastHost(String host, {dynamic customPrefs}) async {
    _settings = _settings.copyWith(lastHostAddress: host);
    notifyListeners();
  }

  Future<void> resetSettings() async {
    _settings = const StreamSettings();
    notifyListeners();
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('com.secondscreen/decoder_control');
  final List<MethodCall> methodLog = <MethodCall>[];

  setUp(() {
    methodLog.clear();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall call) async {
      methodLog.add(call);
      return true;
    });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  testWidgets(
      'StreamingScreen enables keepScreenOn on enter and disables on dispose',
      (tester) async {
    final connectionService = LifecycleMockConnectionService();
    final settingsService = LifecycleMockSettingsService();
    final decoderBridge = SecondScreenDecoderBridge();

    await tester.pumpWidget(
      MaterialApp(
        home: StreamingScreen(
          connectionService: connectionService,
          settingsService: settingsService,
          decoderBridge: decoderBridge,
        ),
      ),
    );

    // Initial keepScreenOn should be true
    expect(
      methodLog.any((c) => c.method == 'setKeepScreenOn' && c.arguments['enable'] == true),
      isTrue,
    );

    // Dispose by popping
    await tester.pumpWidget(const SizedBox());

    // Disposed keepScreenOn should be false
    expect(
      methodLog.any((c) => c.method == 'setKeepScreenOn' && c.arguments['enable'] == false),
      isTrue,
    );

    decoderBridge.dispose();
  });

  testWidgets(
      'StreamingScreen requests keyframe on resume when socket is still alive',
      (tester) async {
    final connectionService = LifecycleMockConnectionService();
    final settingsService = LifecycleMockSettingsService();
    final decoderBridge = SecondScreenDecoderBridge();

    await tester.pumpWidget(
      MaterialApp(
        home: StreamingScreen(
          connectionService: connectionService,
          settingsService: settingsService,
          decoderBridge: decoderBridge,
        ),
      ),
    );

    connectionService.mockSocketConnected = true;
    final initialKeyframes = connectionService.keyframeRequestCount;

    // Simulate screen off / lock -> paused
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
    await tester.pump();

    // Simulate screen on / unlock -> resumed
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    await tester.pump();

    // Keyframe request should have been dispatched
    expect(connectionService.keyframeRequestCount, equals(initialKeyframes + 1));
    expect(connectionService.reconnectCallCount, equals(0));

    decoderBridge.dispose();
  });

  testWidgets(
      'StreamingScreen triggers automatic reconnect on resume when socket was dropped',
      (tester) async {
    final connectionService = LifecycleMockConnectionService();
    final settingsService = LifecycleMockSettingsService();
    final decoderBridge = SecondScreenDecoderBridge();

    await tester.pumpWidget(
      MaterialApp(
        home: StreamingScreen(
          connectionService: connectionService,
          settingsService: settingsService,
          decoderBridge: decoderBridge,
        ),
      ),
    );

    // Simulate socket dropped during suspension / screen off
    await connectionService.disconnect(reason: 'Connection dropped by host.');
    await tester.pump();

    expect(connectionService.mockSocketConnected, isFalse);

    // Simulate phone wake up -> resumed
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    await tester.pump();

    // Reconnection should have been initiated automatically
    expect(connectionService.reconnectCallCount, equals(1));

    decoderBridge.dispose();
  });
}

