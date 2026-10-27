import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../models/connection_state.dart';
import '../models/pc_device.dart';

typedef SocketConnector = Future<Socket> Function(
  dynamic host,
  int port, {
  Duration? timeout,
});

/// Interface for connection lifecycle management.
abstract class ConnectionService extends ChangeNotifier {
  ConnectionStateModel get state;

  Future<bool> connectToDevice(String address, int port, {String? pcName});
  Future<void> disconnect({String? reason});
  void cancelConnection();
}

/// Standard implementation of [ConnectionService].
///
/// In this Phase 2 MVP, [connectToDevice] validates inputs, enforces timeouts,
/// and attempts a real TCP socket handshake to verify host reachability.
/// When the TCP endpoint connects, it holds the state as connected.
///
/// NOTE: As documented in the architecture, a reachable TCP endpoint proves network
/// connectivity to the host; the full video pipeline and IddCx driver integration
/// will plug into this connection in Phases 3, 4, and 5.
class DefaultConnectionService extends ChangeNotifier
    implements ConnectionService {
  final SocketConnector _socketConnector;

  ConnectionStateModel _state = const ConnectionStateModel();
  Socket? _activeSocket;
  bool _isCancelled = false;

  DefaultConnectionService({SocketConnector? socketConnector})
    : _socketConnector = socketConnector ?? Socket.connect;

  @override
  ConnectionStateModel get state => _state;

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
        'ConnectionService: Probing TCP endpoint at $cleanAddress:$port...',
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

      _updateState(
        _state.copyWith(
          status: ConnectionStatus.connected,
          connectedSince: DateTime.now(),
          pingMs: stopwatch.elapsedMilliseconds,
          clearError: true,
        ),
      );

      debugPrint(
        'ConnectionService: TCP endpoint reached in ${stopwatch.elapsedMilliseconds}ms.',
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
              'Make sure the Windows PC is on the same local Wi-Fi network and any firewall allows port $port.',
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
              'Please verify the PC\'s IP address and ensure both devices are connected to the same router.',
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
    _updateState(
      ConnectionStateModel(
        status: ConnectionStatus.disconnected,
        errorMessage: reason,
        connectedDevice: null,
      ),
    );
    debugPrint(
      'ConnectionService: Disconnected. Reason: ${reason ?? "User disconnected"}',
    );
  }

  void _listenToSocket(Socket socket) {
    socket.listen(
      (data) {
        // Reserved for Phase 3 frame packet parsing
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
    super.dispose();
  }
}
