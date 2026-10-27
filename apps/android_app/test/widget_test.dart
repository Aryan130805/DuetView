import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/app.dart';
import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/models/stream_settings.dart';
import 'package:second_screen_android/screens/connection_screen.dart';
import 'package:second_screen_android/screens/home_screen.dart';
import 'package:second_screen_android/screens/settings_screen.dart';
import 'package:second_screen_android/services/connection_service.dart';
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

    expect(find.text('SecondScreen'), findsOneWidget);
    expect(
      find.text('Turn your Android device into a second display for your PC.'),
      findsOneWidget,
    );
    expect(find.text('Connect to PC'), findsOneWidget);
    expect(find.text('Discover PCs'), findsOneWidget);
    expect(find.text('Manual IP'), findsOneWidget);
    expect(find.text('Disconnected'), findsOneWidget);
  });

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

    expect(find.text('Connected (Waiting for frames)'), findsOneWidget);
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
}
