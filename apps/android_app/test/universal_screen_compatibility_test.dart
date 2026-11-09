import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/models/pc_device.dart';
import 'package:second_screen_android/models/stream_settings.dart';
import 'package:second_screen_android/screens/connection_screen.dart';
import 'package:second_screen_android/screens/guide_screen.dart';
import 'package:second_screen_android/screens/home_screen.dart';
import 'package:second_screen_android/screens/settings_screen.dart';
import 'package:second_screen_android/screens/streaming_screen.dart';
import 'package:second_screen_android/services/connection_service.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';
import 'package:second_screen_android/services/saved_pc_storage.dart';
import 'package:second_screen_android/services/settings_service.dart';
import 'package:second_screen_android/services/url_launcher_service.dart';
import 'package:second_screen_android/theme/app_theme.dart';
import 'package:second_screen_android/widgets/download_pc_card.dart';
import 'package:second_screen_android/widgets/download_pc_guide_sheet.dart';

class MockConnectionService extends ChangeNotifier implements ConnectionService {
  ConnectionStateModel _state = const ConnectionStateModel(
    status: ConnectionStatus.disconnected,
  );
  final ValueNotifier<Uint8List?> _latestFrame = ValueNotifier<Uint8List?>(null);

  @override
  ConnectionStateModel get state => _state;

  @override
  ValueListenable<Uint8List?> get latestFrame => _latestFrame;

  @override
  SecondScreenDecoderBridge? get decoderBridge => null;

  @override
  PcDevice? get lastConnectedDevice => null;

  @override
  bool get isSocketConnected => _state.isConnected || _state.isStreaming;

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
    bool isUsb = false,
  }) async => true;

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

  Future<void> resetToDefaults({dynamic customPrefs}) async {
    _settings = const StreamSettings();
    notifyListeners();
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  late MockConnectionService mockConn;
  late MockSettingsService settingsService;
  late SavedPcStorage storage;

  setUp(() async {
    SharedPreferences.setMockInitialValues({});
    mockConn = MockConnectionService();
    settingsService = MockSettingsService();
    storage = SavedPcStorage();
  });

  tearDown(() {
    UrlLauncherService.testHandler = null;
  });

  Widget buildScreen(Widget child) {
    return MaterialApp(
      theme: AppTheme.darkTheme,
      home: child,
    );
  }

  group('Universal Android Compatibility & Responsiveness Tests', () {
    final testViewports = [
      {'name': 'Compact Small Phone', 'size': const Size(320, 568)},
      {'name': 'Compact Standard Phone', 'size': const Size(360, 780)},
      {'name': 'Compact Large Phone', 'size': const Size(412, 915)},
      {'name': 'Foldable Folded', 'size': const Size(340, 800)},
      {'name': 'Small Tablet / Foldable Unfolded', 'size': const Size(768, 1024)},
      {'name': 'Large Tablet Landscape', 'size': const Size(1024, 768)},
      {'name': 'Expanded Slate', 'size': const Size(1280, 800)},
    ];

    for (final vp in testViewports) {
      testWidgets('HomeScreen renders without overflow on ${vp['name']} (${(vp['size'] as Size).width}x${(vp['size'] as Size).height})', (
        tester,
      ) async {
        final size = vp['size'] as Size;
        tester.view.physicalSize = size;
        tester.view.devicePixelRatio = 1.0;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);

        await tester.pumpWidget(
          buildScreen(
            HomeScreen(
              connectionService: mockConn,
              settingsService: settingsService,
              savedPcStorage: storage,
            ),
          ),
        );
        await tester.pumpAndSettle();

        expect(find.text('DuetView'), findsWidgets);
        expect(find.text('Download DuetView PC'), findsOneWidget);
        expect(tester.takeException(), isNull);
      });
    }

    testWidgets('Tapping Download DuetView PC card opens guide sheet with steps and releases link', (
      tester,
    ) async {
      String? launchedUrl;
      UrlLauncherService.testHandler = (url) async {
        launchedUrl = url;
        return true;
      };

      await tester.pumpWidget(
        buildScreen(
          HomeScreen(
            connectionService: mockConn,
            settingsService: settingsService,
            savedPcStorage: storage,
          ),
        ),
      );
      await tester.pumpAndSettle();

      // Ensure Download DuetView PC card is visible
      final downloadCard = find.byType(DownloadPcCard);
      expect(downloadCard, findsOneWidget);

      await tester.scrollUntilVisible(downloadCard, 200);
      await tester.tap(downloadCard);
      await tester.pumpAndSettle();

      // Verify DownloadPcGuideSheet appears
      expect(find.byType(DownloadPcGuideSheet), findsOneWidget);
      expect(find.text('Quick Setup Guide'), findsOneWidget);
      expect(find.text('Download the Release'), findsOneWidget);
      expect(find.text('Install or Extract'), findsOneWidget);
      expect(find.text('Launch DuetView PC'), findsOneWidget);
      expect(find.text('Open GitHub Releases'), findsOneWidget);

      // Tap Open GitHub Releases button
      await tester.tap(find.text('Open GitHub Releases'));
      await tester.pumpAndSettle();

      expect(launchedUrl, equals('https://github.com/Aryan130805/DuetView/releases'));
    });

    testWidgets('SettingsScreen About section contains clickable developer and repository links', (
      tester,
    ) async {
      final launchedUrls = <String>[];
      UrlLauncherService.testHandler = (url) async {
        launchedUrls.add(url);
        return true;
      };

      await tester.pumpWidget(
        buildScreen(
          SettingsScreen(settingsService: settingsService),
        ),
      );
      await tester.pumpAndSettle();

      // Scroll to About section
      final devTile = find.text('Aryan Verma (@Aryan130805)\nTap to view GitHub profile');
      await tester.scrollUntilVisible(devTile, 300);
      expect(devTile, findsOneWidget);

      // Tap Developer profile
      await tester.tap(devTile);
      await tester.pumpAndSettle();

      expect(launchedUrls.contains('https://github.com/Aryan130805'), isTrue);

      // Tap Open Source Project tile
      final repoTile = find.text('Explore source code, releases, and documentation on GitHub');
      expect(repoTile, findsOneWidget);

      await tester.tap(repoTile);
      await tester.pumpAndSettle();

      expect(launchedUrls.contains('https://github.com/Aryan130805/DuetView'), isTrue);
    });

    testWidgets('GuideScreen renders PC download banner and responds to Download PC button', (
      tester,
    ) async {
      String? launchedUrl;
      UrlLauncherService.testHandler = (url) async {
        launchedUrl = url;
        return true;
      };

      await tester.pumpWidget(buildScreen(const GuideScreen()));
      await tester.pumpAndSettle();

      expect(find.text('DuetView PC Companion Required'), findsOneWidget);
      final downloadBtn = find.text('Download PC');
      expect(downloadBtn, findsOneWidget);

      await tester.tap(downloadBtn);
      await tester.pumpAndSettle();

      expect(launchedUrl, equals('https://github.com/Aryan130805/DuetView/releases'));
    });

    testWidgets('StreamingScreen adapts to portrait and landscape viewports without overflow', (
      tester,
    ) async {
      mockConn.setState(
        const ConnectionStateModel(
          status: ConnectionStatus.streaming,
          connectedDevice: PcDevice(
            id: 'test-id',
            name: 'Test-PC',
            address: '192.168.1.100',
            port: 52025,
          ),
          activeProtocol: StreamingProtocol.ss01Jpeg,
        ),
      );

      // 1. Landscape (780x360)
      tester.view.physicalSize = const Size(780, 360);
      tester.view.devicePixelRatio = 1.0;
      await tester.pumpWidget(
        buildScreen(
          StreamingScreen(
            connectionService: mockConn,
            settingsService: settingsService,
          ),
        ),
      );
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 100));
      expect(tester.takeException(), isNull);

      // 2. Portrait (360x780)
      tester.view.physicalSize = const Size(360, 780);
      await tester.pumpWidget(
        buildScreen(
          StreamingScreen(
            connectionService: mockConn,
            settingsService: settingsService,
          ),
        ),
      );
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 100));
      expect(tester.takeException(), isNull);

      tester.view.resetPhysicalSize();
      tester.view.resetDevicePixelRatio();
    });

    testWidgets('ConnectionScreen renders cleanly on narrow 320px viewport without overflow', (
      tester,
    ) async {
      tester.view.physicalSize = const Size(320, 568);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);

      await tester.pumpWidget(
        buildScreen(
          ConnectionScreen(
            connectionService: mockConn,
            settingsService: settingsService,
            savedPcStorage: storage,
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Discover PCs'), findsOneWidget);
      expect(find.text('Manual IP'), findsOneWidget);
      expect(tester.takeException(), isNull);
    });
  });
}
