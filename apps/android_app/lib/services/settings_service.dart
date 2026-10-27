import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/stream_settings.dart';

/// Service managing persistent user preferences for streaming and connection.
class SettingsService extends ChangeNotifier {
  static const String _storageKey = 'second_screen_stream_settings';

  StreamSettings _settings = const StreamSettings();
  bool _isLoaded = false;

  StreamSettings get settings => _settings;
  bool get isLoaded => _isLoaded;

  /// Loads saved settings from storage. If unavailable, keeps defaults.
  Future<void> loadSettings({SharedPreferences? customPrefs}) async {
    try {
      final prefs = customPrefs ?? await SharedPreferences.getInstance();
      final raw = prefs.getString(_storageKey);
      if (raw != null && raw.isNotEmpty) {
        final Map<String, dynamic> decoded =
            jsonDecode(raw) as Map<String, dynamic>;
        _settings = StreamSettings.fromJson(decoded);
      }
    } catch (e) {
      debugPrint(
        'SettingsService: Failed to read settings, using defaults: $e',
      );
    } finally {
      _isLoaded = true;
      notifyListeners();
    }
  }

  /// Updates settings in memory and writes to persistent storage.
  Future<void> updateSettings(
    StreamSettings newSettings, {
    SharedPreferences? customPrefs,
  }) async {
    _settings = newSettings;
    notifyListeners();

    try {
      final prefs = customPrefs ?? await SharedPreferences.getInstance();
      final raw = jsonEncode(_settings.toJson());
      await prefs.setString(_storageKey, raw);
    } catch (e) {
      debugPrint('SettingsService: Failed to persist settings: $e');
    }
  }

  /// Convenience helper to save last successfully targeted PC address.
  Future<void> saveLastHost(
    String host, {
    SharedPreferences? customPrefs,
  }) async {
    if (_settings.lastHostAddress == host) return;
    await updateSettings(
      _settings.copyWith(lastHostAddress: host),
      customPrefs: customPrefs,
    );
  }
}
