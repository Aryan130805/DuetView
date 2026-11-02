import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/app.dart';
import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/models/pc_device.dart';
import 'package:second_screen_android/models/stream_settings.dart';
import 'package:second_screen_android/screens/connection_screen.dart';
import 'package:second_screen_android/screens/guide_screen.dart';
import 'package:second_screen_android/screens/home_screen.dart';
import 'package:second_screen_android/screens/settings_screen.dart';
import 'package:second_screen_android/services/connection_service.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';
import 'package:second_screen_android/services/settings_service.dart';

class MockConnectionService extends ChangeNotifier
    implements ConnectionService {
  ConnectionStateModel _state = const ConnectionStateModel();
  final ValueNotifier<Uint8List?> _latestFrame = ValueNotifier<Uint8List?>(
    null,
  );

  @override
  ConnectionStateModel get state => _state;

  @override
  ValueListenable<Uint8List?> get latestFrame => _latestFrame;

  @override
  SecondScreenDecoderBridge? get decoderBridge => null;

  @override
  PcDevice? get lastConnectedDevice => null;

  @override
  bool get isSocketConnected => false;

  @override
  bool get isReconnecting => false;

  @override
  Future<bool> reconnect({
    int maxAttempts = 5,
    Duration initialDelay = const Duration(milliseconds: 500),
  }) async => true;

  @override
  Future<bool> sendKeyframeRequest() async => true;

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

  void setState(ConnectionStateModel newState) {
    _state = newState;
    notifyListeners();
  }

  void setLatestFrame(Uint8List? frame) {
    _latestFrame.value = frame;
  }

  @override
  Future<bool> connectToDevice(
    String address,
    int port, {
    String? pcName,
  }) async {
    return true;
  }

  @override
  Future<void> disconnect({String? reason}) async {
    _state = const ConnectionStateModel(status: ConnectionStatus.disconnected);
    _latestFrame.value = null;
    notifyListeners();
  }

  @override
  void cancelConnection() {
    _state = const ConnectionStateModel(status: ConnectionStatus.disconnected);
    notifyListeners();
  }
}

class MockSettingsService extends ChangeNotifier implements SettingsService {
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
}

void main() {
  late MockConnectionService mockConnService;
  late MockSettingsService mockSettingsService;

  setUp(() {
    mockConnService = MockConnectionService();
    mockSettingsService = MockSettingsService();
  });

  Widget createTestWidget() {
    return SecondScreenApp(
      customConnectionService: mockConnService,
      customSettingsService: mockSettingsService,
    );
  }

  testWidgets('HomeScreen renders title, description, and status card', (
    tester,
  ) async {
    await tester.pumpWidget(createTestWidget());
    await tester.pumpAndSettle();

    expect(find.text('DuetView'), findsWidgets);
    expect(
      find.text('Turn your Android device into a second display for your PC.'),
      findsOneWidget,
    );
    expect(find.text('Discover PCs'), findsOneWidget);
    expect(find.text('Scan to Connect'), findsOneWidget);
    expect(find.text('How to Use'), findsOneWidget);
    expect(find.text('Manual IP'), findsOneWidget);
    expect(find.text('Not Connected'), findsOneWidget);
  });

  testWidgets(
    'Navigates from HomeScreen to GuideScreen and displays sections',
    (tester) async {
      await tester.pumpWidget(createTestWidget());
      await tester.pumpAndSettle();

      final guideButton = find.byTooltip('How to Use DuetView');
      expect(guideButton, findsOneWidget);
      await tester.tap(guideButton);
      await tester.pumpAndSettle();

      expect(find.byType(GuideScreen), findsOneWidget);
      expect(find.text('How to Use DuetView'), findsOneWidget);
      expect(find.text('Connect Wirelessly'), findsOneWidget);
      expect(find.text('Connect with USB'), findsOneWidget);

      await tester.pageBack();
      await tester.pumpAndSettle();
      expect(find.byType(HomeScreen), findsOneWidget);
    },
  );

  testWidgets(
    'Navigates from HomeScreen to SettingsScreen and displays options',
    (tester) async {
      await tester.pumpWidget(createTestWidget());
      await tester.pumpAndSettle();

      // Tap settings icon in app bar
      final settingsButton = find.byTooltip('Settings');
      expect(settingsButton, findsOneWidget);
      await tester.tap(settingsButton);
      await tester.pumpAndSettle();

      expect(find.byType(SettingsScreen), findsOneWidget);
      expect(find.text('Display Preferences'), findsOneWidget);
      expect(find.text('Preferred Resolution'), findsOneWidget);
      expect(find.text('Preferred Frame Rate'), findsOneWidget);
      expect(find.text('1.0.0 (MVP Build)'), findsOneWidget);

      // Pop back to home
      await tester.pageBack();
      await tester.pumpAndSettle();
      expect(find.byType(HomeScreen), findsOneWidget);
    },
  );

  testWidgets('Navigates from HomeScreen to ConnectionScreen with manual tab', (
    tester,
  ) async {
    await tester.pumpWidget(createTestWidget());
    await tester.pumpAndSettle();

    // Tap 'Manual IP' button
    final manualButton = find.text('Manual IP');
    expect(manualButton, findsOneWidget);
    await tester.ensureVisible(manualButton);
    await tester.tap(manualButton);
    await tester.pumpAndSettle();

    expect(find.byType(ConnectionScreen), findsOneWidget);
    expect(find.text('Windows PC IPv4 Address'), findsOneWidget);
    expect(find.text('Port'), findsOneWidget);
  });

  testWidgets('ConnectionStatusCard responds to connected state', (
    tester,
  ) async {
    await tester.pumpWidget(createTestWidget());
    await tester.pumpAndSettle();

    mockConnService.setState(
      const ConnectionStateModel(
        status: ConnectionStatus.connected,
        pingMs: 12,
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('Connected (Ready to Stream)'), findsOneWidget);
    expect(find.text('12 ms'), findsOneWidget);
    expect(find.text('Open Display'), findsOneWidget);
    expect(find.text('Disconnect'), findsOneWidget);
  });

  testWidgets(
    'ConnectionStatusCard responds to streaming state with live FPS',
    (tester) async {
      await tester.pumpWidget(createTestWidget());
      await tester.pumpAndSettle();

      mockConnService.setState(
        const ConnectionStateModel(
          status: ConnectionStatus.streaming,
          receivedFps: 24,
          lastFrameSizeBytes: 85000,
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Streaming Live'), findsOneWidget);
      expect(find.text('24 FPS'), findsOneWidget);
      expect(find.text('View Live Stream'), findsWidgets);
      expect(find.text('Disconnect'), findsOneWidget);
    },
  );

  testWidgets(
    'SettingsScreen renders Display Preferences without overflow on standard and narrow viewports',
    (tester) async {
      // 1. Test on a compact 320x640 mobile screen
      tester.view.physicalSize = const Size(320, 640);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);

      await tester.pumpWidget(
        MaterialApp(
          home: SettingsScreen(settingsService: mockSettingsService),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Display Preferences'), findsOneWidget);
      expect(find.text('Preferred Resolution'), findsOneWidget);
      expect(find.text('1920 × 1080 (FHD)'), findsOneWidget);
      expect(find.text('Preferred Frame Rate'), findsOneWidget);
      expect(find.text('60 FPS (Smooth)'), findsOneWidget);
      expect(find.text('Full-Screen Mode'), findsOneWidget);

      // Verify no RenderFlex overflow exceptions occurred during compact layout
      expect(tester.takeException(), isNull);

      // 2. Test on a landscape/tablet 1024x768 screen
      tester.view.physicalSize = const Size(1024, 768);
      await tester.pumpWidget(
        MaterialApp(
          home: SettingsScreen(settingsService: mockSettingsService),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Display Preferences'), findsOneWidget);
      expect(find.text('1920 × 1080 (FHD)'), findsOneWidget);
      expect(find.text('60 FPS (Smooth)'), findsOneWidget);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'SettingsScreen updates resolution, frame rate, and full screen preferences interactively',
    (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: SettingsScreen(settingsService: mockSettingsService),
        ),
      );
      await tester.pumpAndSettle();

      // Open resolution dropdown
      final resDropdown = find.text('1920 × 1080 (FHD)');
      expect(resDropdown, findsOneWidget);
      await tester.tap(resDropdown);
      await tester.pumpAndSettle();

      // Select 720p HD option
      final hdOption = find.text('1280 × 720 (HD)').last;
      await tester.tap(hdOption);
      await tester.pumpAndSettle();
      expect(mockSettingsService.settings.resolution, StreamResolution.res720p);

      // Open frame rate dropdown
      final fpsDropdown = find.text('60 FPS (Smooth)');
      expect(fpsDropdown, findsOneWidget);
      await tester.tap(fpsDropdown);
      await tester.pumpAndSettle();

      // Select 30 FPS Power Saver option
      final fps30Option = find.text('30 FPS (Power Saver)').last;
      await tester.tap(fps30Option);
      await tester.pumpAndSettle();
      expect(mockSettingsService.settings.frameRate, StreamFrameRate.fps30);

      // Toggle full-screen mode switch
      final switchFinder = find.byType(Switch).first;
      expect(switchFinder, findsOneWidget);
      await tester.tap(switchFinder);
      await tester.pumpAndSettle();
      expect(mockSettingsService.settings.fullScreen, isFalse);
    },
  );

  testWidgets(
    'HomeScreen renders mode selector and toggles between Wireless and USB mode',
    (tester) async {
      await tester.pumpWidget(createTestWidget());
      await tester.pumpAndSettle();

      // 1. In default Wireless mode:
      expect(find.byKey(const Key('mode_wireless')), findsOneWidget);
      expect(find.byKey(const Key('mode_usb')), findsOneWidget);

      // Verify Wireless options are visible
      expect(find.text('Discover PCs'), findsOneWidget);
      expect(find.text('Scan to Connect'), findsOneWidget);
      expect(find.text('Manual IP'), findsOneWidget);

      // Verify USB card is not visible yet
      expect(find.text('Wired USB Connection'), findsNothing);
      expect(find.byKey(const Key('usb_connect_button')), findsNothing);

      // 2. Switch to USB mode
      final usbTab = find.byKey(const Key('mode_usb'));
      await tester.tap(usbTab);
      await tester.pumpAndSettle();

      // Verify Wireless options are removed
      expect(find.text('Discover PCs'), findsNothing);
      expect(find.text('Scan to Connect'), findsNothing);
      expect(find.text('Manual IP'), findsNothing);

      // Verify Wired USB Connection options are shown
      expect(find.text('Wired USB Connection'), findsOneWidget);
      expect(find.text('Connect via USB'), findsOneWidget);
      expect(find.byKey(const Key('usb_connect_button')), findsOneWidget);
      expect(find.byKey(const Key('usb_tethering_settings_button')), findsOneWidget);
      expect(find.byKey(const Key('usb_guide_button')), findsOneWidget);

      // 3. Switch back to Wireless mode
      final wirelessTab = find.byKey(const Key('mode_wireless'));
      await tester.tap(wirelessTab);
      await tester.pumpAndSettle();

      // Verify Wireless options are restored
      expect(find.text('Discover PCs'), findsOneWidget);
      expect(find.text('Scan to Connect'), findsOneWidget);
      expect(find.text('Manual IP'), findsOneWidget);

      // Verify Wired USB Connection is removed
      expect(find.text('Wired USB Connection'), findsNothing);
      expect(find.byKey(const Key('usb_connect_button')), findsNothing);
    },
  );

  testWidgets(
    'HomeScreen USB mode Connect button triggers device connection',
    (tester) async {
      await tester.pumpWidget(createTestWidget());
      await tester.pumpAndSettle();

      // Switch to USB mode
      await tester.tap(find.byKey(const Key('mode_usb')));
      await tester.pumpAndSettle();

      // Tap 'Connect via USB'
      final connectBtn = find.byKey(const Key('usb_connect_button'));
      expect(connectBtn, findsOneWidget);
      await tester.ensureVisible(connectBtn);
      await tester.tap(connectBtn);
      await tester.pumpAndSettle();

      // Verify interaction succeeds without uncaught exception
      expect(tester.takeException(), isNull);
    },
  );
}
