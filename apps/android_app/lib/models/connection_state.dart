import 'pc_device.dart';

/// The high-level connection state of SecondScreen client.
enum ConnectionStatus {
  disconnected,
  searching,
  connecting,
  connected, // TCP socket connected, waiting for first frame
  streaming, // Frames actively arriving and rendering
  failed,
}

/// Immutable state snapshot of the active connection.
class ConnectionStateModel {
  final ConnectionStatus status;
  final PcDevice? connectedDevice;
  final String? errorMessage;
  final DateTime? connectedSince;
  final int? pingMs;
  final int? targetFps;
  final int receivedFps;
  final int lastFrameSizeBytes;
  final int totalFramesReceived;
  final int? hostTimestampMs;

  const ConnectionStateModel({
    this.status = ConnectionStatus.disconnected,
    this.connectedDevice,
    this.errorMessage,
    this.connectedSince,
    this.pingMs,
    this.targetFps,
    this.receivedFps = 0,
    this.lastFrameSizeBytes = 0,
    this.totalFramesReceived = 0,
    this.hostTimestampMs,
  });

  bool get isConnected => status == ConnectionStatus.connected;
  bool get isStreaming => status == ConnectionStatus.streaming;
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
    int? receivedFps,
    int? lastFrameSizeBytes,
    int? totalFramesReceived,
    int? hostTimestampMs,
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
      receivedFps: receivedFps ?? this.receivedFps,
      lastFrameSizeBytes: lastFrameSizeBytes ?? this.lastFrameSizeBytes,
      totalFramesReceived: totalFramesReceived ?? this.totalFramesReceived,
      hostTimestampMs: hostTimestampMs ?? this.hostTimestampMs,
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
        other.targetFps == targetFps &&
        other.receivedFps == receivedFps &&
        other.lastFrameSizeBytes == lastFrameSizeBytes &&
        other.totalFramesReceived == totalFramesReceived &&
        other.hostTimestampMs == hostTimestampMs;
  }

  @override
  int get hashCode => Object.hash(
    status,
    connectedDevice,
    errorMessage,
    connectedSince,
    pingMs,
    targetFps,
    receivedFps,
    lastFrameSizeBytes,
    totalFramesReceived,
    hostTimestampMs,
  );
}
