import 'pc_device.dart';

/// The high-level connection state of SecondScreen client.
enum ConnectionStatus { disconnected, searching, connecting, connected, failed }

/// Immutable state snapshot of the active connection.
class ConnectionStateModel {
  final ConnectionStatus status;
  final PcDevice? connectedDevice;
  final String? errorMessage;
  final DateTime? connectedSince;
  final int? pingMs;
  final int? targetFps;

  const ConnectionStateModel({
    this.status = ConnectionStatus.disconnected,
    this.connectedDevice,
    this.errorMessage,
    this.connectedSince,
    this.pingMs,
    this.targetFps,
  });

  bool get isConnected => status == ConnectionStatus.connected;
  bool get isConnecting => status == ConnectionStatus.connecting;
  bool get isSearching => status == ConnectionStatus.searching;
  bool get isFailed => status == ConnectionStatus.failed;
  bool get isDisconnected => status == ConnectionStatus.disconnected;

  ConnectionStateModel copyWith({
    ConnectionStatus? status,
    PcDevice? connectedDevice,
    String? errorMessage,
    DateTime? connectedSince,
    int? pingMs,
    int? targetFps,
    bool clearDevice = false,
    bool clearError = false,
  }) {
    return ConnectionStateModel(
      status: status ?? this.status,
      connectedDevice: clearDevice
          ? null
          : (connectedDevice ?? this.connectedDevice),
      errorMessage: clearError ? null : (errorMessage ?? this.errorMessage),
      connectedSince: connectedSince ?? this.connectedSince,
      pingMs: pingMs ?? this.pingMs,
      targetFps: targetFps ?? this.targetFps,
    );
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is ConnectionStateModel &&
        other.status == status &&
        other.connectedDevice == connectedDevice &&
        other.errorMessage == errorMessage &&
        other.connectedSince == connectedSince &&
        other.pingMs == pingMs &&
        other.targetFps == targetFps;
  }

  @override
  int get hashCode => Object.hash(
    status,
    connectedDevice,
    errorMessage,
    connectedSince,
    pingMs,
    targetFps,
  );
}
