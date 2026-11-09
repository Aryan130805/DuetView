import 'dart:async';
import 'package:flutter/material.dart';

import '../models/saved_pc.dart';
import '../screens/streaming_screen.dart';
import '../theme/app_theme.dart';
import 'connection_service.dart';
import 'decoder_bridge.dart';
import 'pc_health_service.dart';
import 'saved_pc_storage.dart';
import 'settings_service.dart';
import 'usb_tether_service.dart';

/// Result snapshot of an initiated connection workflow.
class ConnectionResult {
  final bool success;
  final String? errorMessage;
  final String? connectedHost;
  final int? connectedPort;

  const ConnectionResult({
    required this.success,
    this.errorMessage,
    this.connectedHost,
    this.connectedPort,
  });
}

/// Unified connection coordinator consolidating connection initiation across:
/// - Main connection screen (manual IP & discovered list)
/// - Discovered PC cards
/// - Saved PCs section
/// - Wired USB connection
/// - QR pairing scanner
class SharedConnectionManager {
  final ConnectionService connectionService;
  final SettingsService settingsService;
  final SavedPcStorage savedPcStorage;
  final PcHealthService? pcHealthService;
  final SecondScreenDecoderBridge? decoderBridge;

  SharedConnectionManager({
    required this.connectionService,
    required this.settingsService,
    required this.savedPcStorage,
    this.pcHealthService,
    this.decoderBridge,
  });

  /// Unified connection workflow executing validation, socket setup, health coordination,
  /// state transitions, and safe navigation to [StreamingScreen].
  Future<ConnectionResult> connect({
    required BuildContext context,
    required String host,
    required int port,
    String? pcName,
    String? pcId,
    bool isUsb = false,
    bool replaceCurrentRoute = false,
  }) async {
    final cleanHost = host.trim();
    if (cleanHost.isEmpty) {
      const err = 'Host IP address cannot be empty.';
      _showErrorSnackBar(context, err);
      return const ConnectionResult(success: false, errorMessage: err);
    }

    if (port <= 0 || port > 65535) {
      final err = 'Invalid port "$port". Must be between 1 and 65535.';
      _showErrorSnackBar(context, err);
      return ConnectionResult(success: false, errorMessage: err);
    }

    // Prevent connecting to Android's own IP address
    if (await UsbTetherService.isLocalAndroidIp(cleanHost)) {
      final err =
          'Cannot connect to Android\'s own IP ($cleanHost). Please enter the Windows PC\'s IP address shown in DuetView PC.';
      if (context.mounted) _showErrorSnackBar(context, err);
      return ConnectionResult(success: false, errorMessage: err);
    }

    // 1. Prevent duplicate simultaneous connection to the same active endpoint
    if (connectionService.isSocketConnected &&
        (connectionService.state.isStreaming || connectionService.state.isConnected)) {
      final curDev = connectionService.lastConnectedDevice ?? connectionService.state.connectedDevice;
      if (curDev != null && curDev.address == cleanHost && curDev.port == port) {
        debugPrint('SharedConnectionManager: Already connected to $cleanHost:$port. Opening viewport.');
        _navigateToStreamingScreen(context, replace: replaceCurrentRoute);
        return ConnectionResult(success: true, connectedHost: cleanHost, connectedPort: port);
      }
      // If connected to another device, disconnect cleanly first
      await connectionService.disconnect();
    }

    // 2. Pause health checks and designate actively streaming target so background pings never collide on port 52025
    pcHealthService?.pauseHealthChecks();
    pcHealthService?.setActivelyStreamingPc(pcId, host: cleanHost);

    // 3. Persist last successful host target in settings
    await settingsService.saveLastHost(cleanHost);

    // 4. Initiate transport and socket connection
    debugPrint('SharedConnectionManager: Connecting to $cleanHost:$port (pcId: $pcId, isUsb: $isUsb)...');
    final success = await connectionService.connectToDevice(
      cleanHost,
      port,
      pcName: pcName,
      isUsb: isUsb,
    );

    if (!success) {
      pcHealthService?.setActivelyStreamingPc(null);
      pcHealthService?.resumeHealthChecks();
      if (pcId != null) {
        await savedPcStorage.updatePcStatus(pcId, PcOnlineStatus.offline);
      }
      final err = connectionService.state.errorMessage ??
          'Could not connect to $cleanHost:$port. Ensure DuetView PC is running.';
      if (context.mounted) {
        _showErrorSnackBar(context, err);
      }
      return ConnectionResult(success: false, errorMessage: err);
    }

    // 5. Update or persist SavedPcStorage if connecting to a known saved PC
    if (pcId != null) {
      await savedPcStorage.updatePcStatus(
        pcId,
        PcOnlineStatus.streaming,
        reachableEndpoint: cleanHost,
      );
    }

    // 6. Navigate to StreamingScreen
    if (context.mounted) {
      _navigateToStreamingScreen(context, replace: replaceCurrentRoute);
    }

    return ConnectionResult(
      success: true,
      connectedHost: cleanHost,
      connectedPort: port,
    );
  }

  void _navigateToStreamingScreen(BuildContext context, {bool replace = false}) {
    final route = MaterialPageRoute(
      builder: (ctx) => StreamingScreen(
        connectionService: connectionService,
        settingsService: settingsService,
        decoderBridge: decoderBridge ?? connectionService.decoderBridge,
      ),
    );

    if (replace) {
      Navigator.pushReplacement(context, route).then((_) {
        _onStreamingScreenExited();
      });
    } else {
      Navigator.push(context, route).then((_) {
        _onStreamingScreenExited();
      });
    }
  }

  void _onStreamingScreenExited() {
    pcHealthService?.setActivelyStreamingPc(null);
    pcHealthService?.resumeHealthChecks();
  }

  void _showErrorSnackBar(BuildContext context, String message) {
    if (!context.mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: AppTheme.error,
        duration: const Duration(seconds: 4),
      ),
    );
  }
}
