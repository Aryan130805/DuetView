import 'package:flutter/material.dart';

import 'screens/home_screen.dart';
import 'services/connection_service.dart';
import 'services/decoder_bridge.dart';
import 'services/pc_health_service.dart';
import 'services/saved_pc_storage.dart';
import 'services/settings_service.dart';
import 'theme/app_theme.dart';

class SecondScreenApp extends StatefulWidget {
  final ConnectionService? customConnectionService;
  final SettingsService? customSettingsService;
  final SecondScreenDecoderBridge? customDecoderBridge;
  final SavedPcStorage? customSavedPcStorage;

  const SecondScreenApp({
    super.key,
    this.customConnectionService,
    this.customSettingsService,
    this.customDecoderBridge,
    this.customSavedPcStorage,
  });

  @override
  State<SecondScreenApp> createState() => _SecondScreenAppState();
}

class _SecondScreenAppState extends State<SecondScreenApp> {
  late final SecondScreenDecoderBridge _decoderBridge;
  late final ConnectionService _connectionService;
  late final SettingsService _settingsService;
  late final SavedPcStorage _savedPcStorage;
  late final PcHealthService _pcHealthService;

  @override
  void initState() {
    super.initState();
    _decoderBridge = widget.customDecoderBridge ?? SecondScreenDecoderBridge();
    _connectionService =
        widget.customConnectionService ??
        DefaultConnectionService(decoderBridge: _decoderBridge);
    _settingsService = widget.customSettingsService ?? SettingsService();
    _settingsService.loadSettings();

    _savedPcStorage = widget.customSavedPcStorage ?? SavedPcStorage();
    _savedPcStorage.load();

    _pcHealthService = PcHealthService(storage: _savedPcStorage);
    _pcHealthService.start();
  }

  @override
  void dispose() {
    _pcHealthService.dispose();
    if (widget.customSavedPcStorage == null) {
      _savedPcStorage.dispose();
    }
    if (widget.customConnectionService == null) {
      _connectionService.dispose();
    }
    if (widget.customSettingsService == null) {
      _settingsService.dispose();
    }
    if (widget.customDecoderBridge == null) {
      _decoderBridge.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'DuetView',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.lightTheme,
      darkTheme: AppTheme.darkTheme,
      themeMode: ThemeMode.system,
      home: HomeScreen(
        connectionService: _connectionService,
        settingsService: _settingsService,
        decoderBridge: _decoderBridge,
        savedPcStorage: _savedPcStorage,
        pcHealthService: _pcHealthService,
      ),
    );
  }
}
