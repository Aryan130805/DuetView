import 'package:flutter/material.dart';

import 'screens/home_screen.dart';
import 'services/connection_service.dart';
import 'services/settings_service.dart';
import 'theme/app_theme.dart';

class SecondScreenApp extends StatefulWidget {
  final ConnectionService? customConnectionService;
  final SettingsService? customSettingsService;

  const SecondScreenApp({
    super.key,
    this.customConnectionService,
    this.customSettingsService,
  });

  @override
  State<SecondScreenApp> createState() => _SecondScreenAppState();
}

class _SecondScreenAppState extends State<SecondScreenApp> {
  late final ConnectionService _connectionService;
  late final SettingsService _settingsService;

  @override
  void initState() {
    super.initState();
    _connectionService =
        widget.customConnectionService ?? DefaultConnectionService();
    _settingsService = widget.customSettingsService ?? SettingsService();
    _settingsService.loadSettings();
  }

  @override
  void dispose() {
    // Only dispose if we created them locally
    if (widget.customConnectionService == null) {
      _connectionService.dispose();
    }
    if (widget.customSettingsService == null) {
      _settingsService.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SecondScreen',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.lightTheme,
      darkTheme: AppTheme.darkTheme,
      themeMode: ThemeMode.system,
      home: HomeScreen(
        connectionService: _connectionService,
        settingsService: _settingsService,
      ),
    );
  }
}
