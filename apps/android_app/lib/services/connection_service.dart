import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../models/connection_state.dart';
import '../models/pc_device.dart';
import 'frame_accumulator.dart';

typedef SocketConnector = Future<Socket> Function(
  dynamic host,
  int port, {
  Duration? timeout,
});

/// Interface for connection lifecycle and video frame reception management.
abstract class ConnectionService extends ChangeNotifier {
  ConnectionStateModel get state;
  ValueListenable<Uint8List?> get latestFrame;

  Future<bool> connectToDevice(String address, int port, {String? pcName});
  Future<void> disconnect({String? reason});
  void cancelConnection();
}

/// Standard implementation of [ConnectionService].
///
/// Connects to the SecondScreen streaming host via TCP, validates the v1
/// framing protocol, and yields received JPEG frames to [latestFrame] for display.
class DefaultConnectionService extends ChangeNotifier
    implements ConnectionService {
  final SocketConnector _socketConnector;

  ConnectionStateModel _state = const ConnectionStateModel();
  final ValueNotifier<Uint8List?> _latestFrame = ValueNotifier<Uint8List?>(
    null,
  );

  Socket? _activeSocket;
  FrameAccumulator? _accumulator;
  bool _isCancelled = false;

  final List<int> _recentFrameTimestamps = [];

  DefaultConnectionService({SocketConnector? socketConnector})
    : _socketConnector = socketConnector ?? Socket.connect;

  @override
  ConnectionStateModel get state => _state;

  @override
  ValueListenable<Uint8List?> get latestFrame => _latestFrame;

  /// Validates an IPv4 address string (e.g. 192.168.1.100).
  static bool isValidIpv4(String address) {
    final trimmed = address.trim();
    if (trimmed.isEmpty) return false;
    final regex = RegExp(
      r'^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.'
      r'(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.'
      r'(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.'
      r'(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$',
    );
    return regex.hasMatch(trimmed);
  }

  /// Validates port number (1 - 65535).
  static bool isValidPort(int port) {
    return port >= 1 && port <= 65535;
  }

  @override
  Future<bool> connectToDevice(
    String address,
    int port, {
    String? pcName,
  }) async {
    final cleanAddress = address.trim();

    // 1. Validate inputs
    if (!isValidIpv4(cleanAddress)) {
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage:
              'Invalid IPv4 address "$cleanAddress". Please enter a valid address (e.g. 192.168.1.100).',
          clearDevice: true,
        ),
      );
      return false;
    }

    if (!isValidPort(port)) {
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage:
              'Invalid port "$port". Port must be between 1 and 65535.',
          clearDevice: true,
        ),
      );
      return false;
    }

    // 2. Prevent overlapping attempts
    if (_state.status == ConnectionStatus.connecting) {
      debugPrint('ConnectionService: Connection attempt already in progress.');
      return false;
    }

    // Close any previous socket
    await _closeSocket();

    _isCancelled = false;
    _recentFrameTimestamps.clear();
    _latestFrame.value = null;

    final targetDevice = PcDevice(
      id: '$cleanAddress:$port',
      name: pcName ?? 'Windows PC ($cleanAddress)',
      address: cleanAddress,
      port: port,
      lastSeen: DateTime.now(),
    );

    _updateState(
      ConnectionStateModel(
        status: ConnectionStatus.connecting,
        connectedDevice: targetDevice,
        errorMessage: null,
      ),
    );

    final stopwatch = Stopwatch()..start();

    try {
      debugPrint(
        'ConnectionService: Connecting to host at $cleanAddress:$port...',
      );
      final socket = await _socketConnector(
        cleanAddress,
        port,
        timeout: const Duration(seconds: 4),
      );

      stopwatch.stop();

      if (_isCancelled) {
        socket.destroy();
        _updateState(
          const ConnectionStateModel(
            status: ConnectionStatus.disconnected,
            errorMessage: 'Connection attempt cancelled by user.',
          ),
        );
        return false;
      }

      _activeSocket = socket;
      _listenToSocket(socket);

      // Connected to TCP socket; now waiting for incoming stream frames
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.connected,
          connectedSince: DateTime.now(),
          pingMs: stopwatch.elapsedMilliseconds,
          clearError: true,
        ),
      );

      debugPrint(
        'ConnectionService: TCP connection open in ${stopwatch.elapsedMilliseconds}ms. Awaiting stream frames...',
      );
      return true;
    } on SocketException catch (e) {
      if (_isCancelled) return false;
      debugPrint('ConnectionService: Socket error: $e');
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage:
              'Could not connect to $cleanAddress:$port. '
              'Make sure the Windows PC is on the same local Wi-Fi network and the host server is running.',
        ),
      );
      return false;
    } on TimeoutException {
      if (_isCancelled) return false;
      debugPrint('ConnectionService: Connection timed out.');
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage:
              'Connection timed out after 4 seconds. '
              'Please verify the PC\'s IP address and ensure Windows Firewall allows port $port.',
        ),
      );
      return false;
    } catch (e) {
      if (_isCancelled) return false;
      debugPrint('ConnectionService: Unexpected error during connect: $e');
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage: 'Connection error: ${e.toString()}',
        ),
      );
      return false;
    }
  }

  void _onFrameReceived(Uint8List frameBytes, int timestampMs) {
    _latestFrame.value = frameBytes;

    final now = DateTime.now().millisecondsSinceEpoch;
    _recentFrameTimestamps.add(now);
    _recentFrameTimestamps.removeWhere((ts) => now - ts > 1000);
    final currentFps = _recentFrameTimestamps.length;

    _updateState(
      _state.copyWith(
        status: ConnectionStatus.streaming,
        receivedFps: currentFps,
        lastFrameSizeBytes: frameBytes.length,
        totalFramesReceived: _state.totalFramesReceived + 1,
        hostTimestampMs: timestampMs,
        clearError: true,
      ),
    );
  }

  void _listenToSocket(Socket socket) {
    _accumulator = FrameAccumulator(
      onFrame: _onFrameReceived,
      onError: (err) {
        debugPrint('ConnectionService: $err');
        disconnect(reason: err);
      },
    );

    socket.listen(
      (data) {
        _accumulator?.addChunk(data);
      },
      onError: (error) {
        debugPrint('ConnectionService: Socket stream error: $error');
        disconnect(reason: 'Connection dropped by host.');
      },
      onDone: () {
        debugPrint('ConnectionService: Host closed connection.');
        disconnect(reason: 'Host closed the connection.');
      },
      cancelOnError: true,
    );
  }

  @override
  void cancelConnection() {
    if (_state.status == ConnectionStatus.connecting) {
      _isCancelled = true;
      _updateState(
        const ConnectionStateModel(
          status: ConnectionStatus.disconnected,
          errorMessage: 'Connection attempt cancelled.',
        ),
      );
    }
  }

  @override
  Future<void> disconnect({String? reason}) async {
    await _closeSocket();
    _accumulator?.reset();
    _accumulator = null;
    _recentFrameTimestamps.clear();
    _latestFrame.value = null;

    _updateState(
      ConnectionStateModel(
        status: reason != null
            ? ConnectionStatus.failed
            : ConnectionStatus.disconnected,
        errorMessage: reason,
        connectedDevice: null,
      ),
    );
    debugPrint(
      'ConnectionService: Disconnected. Reason: ${reason ?? "User disconnected"}',
    );
  }

  Future<void> _closeSocket() async {
    if (_activeSocket != null) {
      try {
        await _activeSocket?.close();
        _activeSocket?.destroy();
      } catch (_) {}
      _activeSocket = null;
    }
  }

  void _updateState(ConnectionStateModel newState) {
    _state = newState;
    notifyListeners();
  }

  @override
  void dispose() {
    _closeSocket();
    _latestFrame.dispose();
    super.dispose();
  }
}
