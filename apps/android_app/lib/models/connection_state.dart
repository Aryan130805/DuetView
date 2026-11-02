import 'pc_device.dart';

/// The active streaming protocol mode.
enum StreamingProtocol {
  none,
  ss02H264, // Production H.264 MediaCodec hardware streaming (Phase 4)
  ss01Jpeg, // Fallback JPEG TCP streaming prototype (Phase 3)
}

/// The high-level connection state of SecondScreen client.
enum ConnectionStatus {
  disconnected,
  searching,
  connecting,
  connected, // TCP socket connected, waiting for codec config / first frame
  streaming, // Frames actively arriving and rendering
  failed,
}

/// Immutable state snapshot of the active connection.
class ConnectionStateModel {
  final ConnectionStatus status;
  final StreamingProtocol activeProtocol;
  final PcDevice? connectedDevice;
  final String? errorMessage;
  final DateTime? connectedSince;
  final int? pingMs;
  final int? targetFps;
  final int receivedFps;
  final int lastFrameSizeBytes;
  final int totalFramesReceived;
  final int? hostTimestampMs;

  // Milestone E Metrics
  final bool hasCodecConfig;
  final bool hasReceivedFirstKeyframe;
  final int keyframeRequestsSent;
  final int totalBytesReceived;
  final int totalBytesTransmitted;
  final int configPacketsReceived;
  final int keyframePacketsReceived;
  final int deltaPacketsReceived;
  final String? fallbackReason;

  const ConnectionStateModel({
    this.status = ConnectionStatus.disconnected,
    this.activeProtocol = StreamingProtocol.none,
    this.connectedDevice,
    this.errorMessage,
    this.connectedSince,
    this.pingMs,
    this.targetFps,
    this.receivedFps = 0,
    this.lastFrameSizeBytes = 0,
    this.totalFramesReceived = 0,
    this.hostTimestampMs,
    this.hasCodecConfig = false,
    this.hasReceivedFirstKeyframe = false,
    this.keyframeRequestsSent = 0,
    this.totalBytesReceived = 0,
    this.totalBytesTransmitted = 0,
    this.configPacketsReceived = 0,
    this.keyframePacketsReceived = 0,
    this.deltaPacketsReceived = 0,
    this.fallbackReason,
  });

  bool get isConnected => status == ConnectionStatus.connected;
  bool get isStreaming => status == ConnectionStatus.streaming;
  bool get isConnecting => status == ConnectionStatus.connecting;
  bool get isSearching => status == ConnectionStatus.searching;
  bool get isFailed => status == ConnectionStatus.failed;
  bool get isDisconnected => status == ConnectionStatus.disconnected;

  bool get isH264Streaming =>
      isStreaming && activeProtocol == StreamingProtocol.ss02H264;
  bool get isJpegStreaming =>
      isStreaming && activeProtocol == StreamingProtocol.ss01Jpeg;

  ConnectionStateModel copyWith({
    ConnectionStatus? status,
    StreamingProtocol? activeProtocol,
    PcDevice? connectedDevice,
    String? errorMessage,
    DateTime? connectedSince,
    int? pingMs,
    int? targetFps,
    int? receivedFps,
    int? lastFrameSizeBytes,
    int? totalFramesReceived,
    int? hostTimestampMs,
    bool? hasCodecConfig,
    bool? hasReceivedFirstKeyframe,
    int? keyframeRequestsSent,
    int? totalBytesReceived,
    int? totalBytesTransmitted,
    int? configPacketsReceived,
    int? keyframePacketsReceived,
    int? deltaPacketsReceived,
    String? fallbackReason,
    bool clearDevice = false,
    bool clearError = false,
  }) {
    return ConnectionStateModel(
      status: status ?? this.status,
      activeProtocol: activeProtocol ?? this.activeProtocol,
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
      hasCodecConfig: hasCodecConfig ?? this.hasCodecConfig,
      hasReceivedFirstKeyframe:
          hasReceivedFirstKeyframe ?? this.hasReceivedFirstKeyframe,
      keyframeRequestsSent: keyframeRequestsSent ?? this.keyframeRequestsSent,
      totalBytesReceived: totalBytesReceived ?? this.totalBytesReceived,
      totalBytesTransmitted:
          totalBytesTransmitted ?? this.totalBytesTransmitted,
      configPacketsReceived:
          configPacketsReceived ?? this.configPacketsReceived,
      keyframePacketsReceived:
          keyframePacketsReceived ?? this.keyframePacketsReceived,
      deltaPacketsReceived: deltaPacketsReceived ?? this.deltaPacketsReceived,
      fallbackReason: fallbackReason ?? this.fallbackReason,
    );
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is ConnectionStateModel &&
        other.status == status &&
        other.activeProtocol == activeProtocol &&
        other.connectedDevice == connectedDevice &&
        other.errorMessage == errorMessage &&
        other.connectedSince == connectedSince &&
        other.pingMs == pingMs &&
        other.targetFps == targetFps &&
        other.receivedFps == receivedFps &&
        other.lastFrameSizeBytes == lastFrameSizeBytes &&
        other.totalFramesReceived == totalFramesReceived &&
        other.hostTimestampMs == hostTimestampMs &&
        other.hasCodecConfig == hasCodecConfig &&
        other.hasReceivedFirstKeyframe == hasReceivedFirstKeyframe &&
        other.keyframeRequestsSent == keyframeRequestsSent &&
        other.totalBytesReceived == totalBytesReceived &&
        other.totalBytesTransmitted == totalBytesTransmitted &&
        other.configPacketsReceived == configPacketsReceived &&
        other.keyframePacketsReceived == keyframePacketsReceived &&
        other.deltaPacketsReceived == deltaPacketsReceived &&
        other.fallbackReason == fallbackReason;
  }

  @override
  int get hashCode => Object.hash(
    status,
    activeProtocol,
    connectedDevice,
    errorMessage,
    connectedSince,
    pingMs,
    targetFps,
    receivedFps,
    lastFrameSizeBytes,
    totalFramesReceived,
    hostTimestampMs,
    hasCodecConfig,
    hasReceivedFirstKeyframe,
    keyframeRequestsSent,
    totalBytesReceived,
    totalBytesTransmitted,
    configPacketsReceived,
    keyframePacketsReceived,
    deltaPacketsReceived,
    fallbackReason,
  );
}
