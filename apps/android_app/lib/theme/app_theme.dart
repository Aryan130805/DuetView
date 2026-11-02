import 'package:flutter/material.dart';

import '../models/connection_state.dart';

class AppTheme {
  // Shared Brand Design System Palette (Matching Reference Images)
  static const Color mainBackground = Color(0xFF080F20);
  static const Color secondaryBackground = Color(0xFF0D1730);
  static const Color primaryCard = Color(0xFF14213B);
  static const Color elevatedCard = Color(0xFF1A2945);
  static const Color primaryBlue = Color(0xFF2688FF);
  static const Color cyanAccent = Color(0xFF42D9FF);
  static const Color violetAccent = Color(0xFF7957FF);
  static const Color primaryText = Color(0xFFF3F6FF);
  static const Color secondaryText = Color(0xFFAAB7D0);
  static const Color mutedText = Color(0xFF7586A6);
  static const Color success = Color(0xFF20D6A0);
  static const Color warning = Color(0xFFFFBD59);
  static const Color error = Color(0xFFFF5D70);
  static const Color border = Color(0xFF2B3D5D);

  // Status colors
  static const Color statusDisconnected = mutedText;
  static const Color statusSearching = warning;
  static const Color statusConnecting = cyanAccent;
  static const Color statusConnected = primaryBlue;
  static const Color statusStreaming = success;
  static const Color statusFailed = error;

  static Color colorForStatus(ConnectionStatus status) {
    switch (status) {
      case ConnectionStatus.disconnected:
        return statusDisconnected;
      case ConnectionStatus.searching:
        return statusSearching;
      case ConnectionStatus.connecting:
        return statusConnecting;
      case ConnectionStatus.connected:
        return statusConnected;
      case ConnectionStatus.streaming:
        return statusStreaming;
      case ConnectionStatus.failed:
        return statusFailed;
    }
  }

  static String labelForStatus(ConnectionStatus status) {
    switch (status) {
      case ConnectionStatus.disconnected:
        return 'Not Connected';
      case ConnectionStatus.searching:
        return 'Searching for PCs...';
      case ConnectionStatus.connecting:
        return 'Connecting to PC...';
      case ConnectionStatus.connected:
        return 'Connected (Ready to Stream)';
      case ConnectionStatus.streaming:
        return 'Streaming Live';
      case ConnectionStatus.failed:
        return 'Connection Failed';
    }
  }

  static ThemeData get darkTheme {
    const colorScheme = ColorScheme(
      brightness: Brightness.dark,
      primary: primaryBlue,
      onPrimary: Colors.white,
      secondary: cyanAccent,
      onSecondary: Colors.black,
      tertiary: violetAccent,
      onTertiary: Colors.white,
      error: error,
      onError: Colors.white,
      surface: primaryCard,
      onSurface: primaryText,
      surfaceContainerHighest: elevatedCard,
      onSurfaceVariant: secondaryText,
      outline: border,
    );

    return ThemeData(
      useMaterial3: true,
      colorScheme: colorScheme,
      scaffoldBackgroundColor: mainBackground,
      appBarTheme: const AppBarTheme(
        centerTitle: false,
        elevation: 0,
        backgroundColor: Colors.transparent,
        foregroundColor: primaryText,
        titleTextStyle: TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.w700,
          color: primaryText,
          letterSpacing: -0.3,
        ),
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        color: primaryCard,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(20),
          side: const BorderSide(color: border),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: elevatedCard,
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(14),
          borderSide: const BorderSide(color: border),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(14),
          borderSide: const BorderSide(color: border),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(14),
          borderSide: const BorderSide(color: cyanAccent, width: 2),
        ),
        contentPadding: const EdgeInsets.symmetric(
          horizontal: 16,
          vertical: 14,
        ),
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(14),
          ),
          textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
        ),
      ),
    );
  }

  static ThemeData get lightTheme => darkTheme;
}
