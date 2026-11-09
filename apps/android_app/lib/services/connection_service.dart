import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../models/connection_state.dart';
import '../models/pc_device.dart';
import 'decoder_bridge.dart';
import 'frame_accumulator.dart';
import 'usb_tether_service.dart';

typedef SocketConnector = Future<Socket> Function(
  dynamic host,
  int port, {
  Duration? timeout,
});

/// Interface for connection lifecycle and video frame reception management.
abstract class ConnectionService extends ChangeNotifier {
  ConnectionStateModel get state;
  ValueListenable<Uint8List?> get latestFrame;
  SecondScreenDecoderBridge? get decoderBridge;
  PcDevice? get lastConnectedDevice;
  bool get isSocketConnected;
  bool get isReconnecting;

  Future<bool> connectToDevice(
    String address,
    int port, {
    String? pcName,
    bool isUsb = false,
  });
  Future<bool> reconnect({
    int maxAttempts = 5,
    Duration initialDelay = const Duration(milliseconds: 500),
  });
  Future<void> disconnect({String? reason});
  void cancelConnection();
  Future<bool> sendKeyframeRequest();
  Future<bool> sendReceiverReport({
    int? receivedFps,
    int? decodedFps,
    int? droppedFrames,
    int? bufferDelayMs,
  });
  Future<bool> sendPointerEvent({
    required PointerEventType type,
    required double normalizedX,
    required double normalizedY,
    PointerButton button = PointerButton.none,
    int modifiers = 0,
    int flags = 0,
  });
  Future<bool> sendScrollEvent({
    required double normalizedX,
    required double normalizedY,
    int deltaX = 0,
    required int deltaY,
  });
  Future<bool> sendKeyboardEvent({
    required KeyEventType type,
    int keyCode = 0,
    int unicode = 0,
    int modifiers = 0,
  });
}

/// Standard implementation of [ConnectionService].
///
/// Connects to the SecondScreen streaming host via TCP, detects whether the stream
/// is SS02 (H.264 video) or SS01 (JPEG fallback), dispatches packets to
/// [SecondScreenDecoderBridge] or yields JPEG frames to [latestFrame] for display.
class DefaultConnectionService extends ChangeNotifier
    implements ConnectionService {
  final SocketConnector _socketConnector;
  @override
  final SecondScreenDecoderBridge? decoderBridge;

  ConnectionStateModel _state = const ConnectionStateModel();
  final ValueNotifier<Uint8List?> _latestFrame = ValueNotifier<Uint8List?>(
    null,
  );

  Socket? _activeSocket;
  FrameAccumulator? _accumulator;
  StreamSubscription<void>? _keyframeRequestSubscription;
  PcDevice? _lastConnectedDevice;
  bool _lastConnectWasUsb = false;
  bool _isReconnecting = false;
  bool _isCancelled = false;
  bool _isClosing = false;
  bool _isDisposed = false;
  int _upstreamSeq = 0;
  int _droppedFramesCount = 0;
  DateTime _lastKeyframeRequestTime = DateTime.fromMillisecondsSinceEpoch(0);
  Timer? _receiverReportTimer;
  Timer? _initialKeyframeTimer;
  int _initialKeyframeRetryCount = 0;

  final List<int> _recentFrameTimestamps = [];

  DefaultConnectionService({
    SocketConnector? socketConnector,
    this.decoderBridge,
  }) : _socketConnector = socketConnector ?? Socket.connect {
    // Listen for decoder-initiated keyframe requests
    _keyframeRequestSubscription = decoderBridge?.onKeyframeRequested.listen((
      _,
    ) {
      sendKeyframeRequest();
    });
  }

  @override
  ConnectionStateModel get state => _state;

  @override
  ValueListenable<Uint8List?> get latestFrame => _latestFrame;

  @override
  PcDevice? get lastConnectedDevice => _lastConnectedDevice;

  @override
  bool get isSocketConnected => _activeSocket != null;

  @override
  bool get isReconnecting => _isReconnecting;

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
    bool isUsb = false,
  }) async {
    _lastConnectWasUsb = isUsb;
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

    // Guard against attempting to connect to Android's own IP address
    if (await UsbTetherService.isLocalAndroidIp(cleanAddress)) {
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage:
              'Cannot connect to Android\'s own IP ($cleanAddress). Please enter the Windows PC\'s IP address shown in DuetView PC.',
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

    // Close any previous socket and reset decoder
    await _closeSocket();
    await decoderBridge?.resetDecoder();

    _isCancelled = false;
    _recentFrameTimestamps.clear();
    _latestFrame.value = null;
    _upstreamSeq = 0;

    // 3. Inspect Android decoder capabilities for H.264 support
    StreamingProtocol targetProtocol = StreamingProtocol.ss02H264;
    String? fallbackReason;

    if (decoderBridge != null) {
      try {
        final caps = await decoderBridge!.getDecoderCapabilities();
        if (!caps.isSupported) {
          targetProtocol = StreamingProtocol.ss01Jpeg;
          fallbackReason =
              'H.264 decoder not supported on this device. Using SS01 JPEG fallback.';
          debugPrint('ConnectionService: $fallbackReason');
        } else {
          // Initialize decoder for initial stream dimensions
          await decoderBridge!.initializeDecoder(width: 1280, height: 720);
        }
      } catch (e) {
        debugPrint('ConnectionService: Error probing decoder capabilities: $e');
      }
    }

    final targetDevice = PcDevice(
      id: '$cleanAddress:$port',
      name: pcName ?? 'Windows PC ($cleanAddress)',
      address: cleanAddress,
      port: port,
      lastSeen: DateTime.now(),
    );
    _lastConnectedDevice = targetDevice;

    _updateState(
      ConnectionStateModel(
        status: ConnectionStatus.connecting,
        activeProtocol: targetProtocol,
        connectedDevice: targetDevice,
        errorMessage: null,
        fallbackReason: fallbackReason,
      ),
    );

    final stopwatch = Stopwatch()..start();

    try {
      debugPrint(
        'ConnectionService: Connecting to host at $cleanAddress:$port ($targetProtocol)...',
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

      // Connected to TCP socket; request initial IDR keyframe and start keyframe recovery timer
      sendKeyframeRequest();
      _startInitialKeyframeRecoveryTimer();

      _updateState(
        _state.copyWith(
          status: ConnectionStatus.connected,
          connectedSince: DateTime.now(),
          pingMs: stopwatch.elapsedMilliseconds,
          clearError: true,
        ),
      );

      debugPrint(
        'ConnectionService: TCP connection open in ${stopwatch.elapsedMilliseconds}ms. Awaiting stream packets...',
      );
      return true;
    } on SocketException catch (e) {
      if (_isCancelled) return false;
      debugPrint('ConnectionService: Socket error: $e');
      final errorMsg = isUsb
          ? 'Cannot connect to PC via USB cable on $cleanAddress:$port. '
            'Please ensure USB Tethering is enabled in Android settings and DuetView PC is running on your computer.'
          : 'Could not connect to $cleanAddress:$port. '
            'Make sure the Windows PC is on the same local Wi-Fi network and the host server is running.';
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage: errorMsg,
        ),
      );
      return false;
    } on TimeoutException {
      if (_isCancelled) return false;
      debugPrint('ConnectionService: Connection timed out.');
      final errorMsg = isUsb
          ? 'Connection to $cleanAddress:$port timed out. '
            'Ensure the USB cable is firmly connected, USB Tethering is active, and Windows Defender Firewall allows port $port.'
          : 'Connection timed out after 4 seconds. '
            'Please verify the PC\'s IP address and ensure Windows Firewall allows port $port.';
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage: errorMsg,
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

  void _onConnectionDropped(String reason) {
    if (_isClosing || _isDisposed) return;
    _cancelInitialKeyframeRecoveryTimer();
    _activeSocket?.destroy();
    _activeSocket = null;
    _receiverReportTimer?.cancel();
    _receiverReportTimer = null;
    _accumulator?.reset();

    final dev = _lastConnectedDevice;
    if (dev != null && !_isCancelled) {
      debugPrint('ConnectionService: Socket disconnected ($reason). Auto-reconnecting...');
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.connecting,
          errorMessage: 'Connection lost ($reason). Reconnecting...',
        ),
      );
      reconnect(maxAttempts: 5).then((success) {
        if (success) {
          sendKeyframeRequest();
        } else {
          _updateState(
            _state.copyWith(
              status: ConnectionStatus.failed,
              errorMessage: 'Connection lost ($reason). Automatic reconnection failed.',
            ),
          );
        }
      });
    } else {
      _updateState(
        _state.copyWith(
          status: ConnectionStatus.failed,
          errorMessage: reason,
        ),
      );
    }
  }

  void _startInitialKeyframeRecoveryTimer() {
    _cancelInitialKeyframeRecoveryTimer();
    _initialKeyframeRetryCount = 0;
    _initialKeyframeTimer = Timer.periodic(
      const Duration(milliseconds: 1200),
      (timer) {
        if (_state.hasReceivedFirstKeyframe || _activeSocket == null) {
          _cancelInitialKeyframeRecoveryTimer();
          return;
        }
        _initialKeyframeRetryCount++;
        if (_initialKeyframeRetryCount <= 4) {
          debugPrint(
            'ConnectionService: Initial keyframe recovery attempt $_initialKeyframeRetryCount/4...',
          );
          sendKeyframeRequest();
        } else {
          _cancelInitialKeyframeRecoveryTimer();
          debugPrint(
            'ConnectionService: Initial keyframe recovery timed out after 4 attempts.',
          );
        }
      },
    );
  }

  void _cancelInitialKeyframeRecoveryTimer() {
    _initialKeyframeTimer?.cancel();
    _initialKeyframeTimer = null;
  }

  static Uint8List? _extractInlineSpsPps(Uint8List payload) {
    if (payload.length < 8) return null;
    final startCodeIndices = <int>[];
    for (int i = 0; i < payload.length - 3; i++) {
      if (payload[i] == 0 && payload[i + 1] == 0) {
        if (payload[i + 2] == 1) {
          startCodeIndices.add(i);
        } else if (payload[i + 2] == 0 &&
            i < payload.length - 4 &&
            payload[i + 3] == 1) {
          startCodeIndices.add(i);
          i++; // skip
        }
      }
    }
    if (startCodeIndices.isEmpty) return null;

    int? spsStart;
    int? spsEnd;
    int? ppsStart;
    int? ppsEnd;

    for (int idx = 0; idx < startCodeIndices.length; idx++) {
      final start = startCodeIndices[idx];
      final prefixLen =
          (start + 3 < payload.length && payload[start + 2] == 1) ? 3 : 4;
      final nalHeaderIdx = start + prefixLen;
      if (nalHeaderIdx >= payload.length) continue;
      final nalType = payload[nalHeaderIdx] & 0x1F;
      final nextStart = (idx + 1 < startCodeIndices.length)
          ? startCodeIndices[idx + 1]
          : payload.length;

      if (nalType == 7) {
        // SPS
        spsStart = start;
        spsEnd = nextStart;
      } else if (nalType == 8) {
        // PPS
        ppsStart = start;
        ppsEnd = nextStart;
      }
    }

    if (spsStart != null &&
        spsEnd != null &&
        ppsStart != null &&
        ppsEnd != null) {
      final sps = payload.sublist(spsStart, spsEnd);
      final pps = payload.sublist(ppsStart, ppsEnd);
      final config = Uint8List(sps.length + pps.length);
      config.setRange(0, sps.length, sps);
      config.setRange(sps.length, config.length, pps);
      return config;
    }
    return null;
  }

  void _listenToSocket(Socket socket) {
    _accumulator = FrameAccumulator(
      onSs02Packet: _onSs02PacketReceived,
      onFrame: _onSs01FrameReceived,
      onError: (err) {
        debugPrint('ConnectionService: $err');
        _onConnectionDropped(err);
      },
    );

    socket.listen(
      (data) {
        _accumulator?.addChunk(data);
      },
      onError: (error) {
        if (!_isClosing && !_isDisposed) {
          debugPrint('ConnectionService: Socket stream error: $error');
          _onConnectionDropped('Connection dropped by host.');
        }
      },
      onDone: () {
        if (!_isClosing && !_isDisposed) {
          debugPrint('ConnectionService: Host closed connection.');
          _onConnectionDropped('Host closed the connection.');
        }
      },
      cancelOnError: true,
    );

    // Periodically send receiver telemetry report to the Windows host
    _receiverReportTimer?.cancel();
    _receiverReportTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      sendReceiverReport();
    });
  }

  void _onSs02PacketReceived(Ss02Packet packet) {
    final now = DateTime.now().millisecondsSinceEpoch;
    final totalBytes =
        _state.totalBytesReceived +
        FrameAccumulator.ss02HeaderSize +
        packet.payloadLength;

    switch (packet.type) {
      case Ss02PacketType.config:
        debugPrint(
          'ConnectionService: Received SS02 CONFIG (${packet.payloadLength} bytes)',
        );
        decoderBridge?.feedConfigPacket(packet.payload);

        _updateState(
          _state.copyWith(
            activeProtocol: StreamingProtocol.ss02H264,
            hasCodecConfig: true,
            totalBytesReceived: totalBytes,
            configPacketsReceived: _state.configPacketsReceived + 1,
            clearError: true,
          ),
        );
        break;

      case Ss02PacketType.keyframe:
        _cancelInitialKeyframeRecoveryTimer();
        if (!_state.hasCodecConfig) {
          final inlineConfig = _extractInlineSpsPps(packet.payload);
          if (inlineConfig != null) {
            debugPrint(
              'ConnectionService: Recovered inline SPS/PPS from keyframe (${inlineConfig.length} bytes)',
            );
            decoderBridge?.feedConfigPacket(inlineConfig);
            _updateState(
              _state.copyWith(
                activeProtocol: StreamingProtocol.ss02H264,
                hasCodecConfig: true,
                configPacketsReceived: _state.configPacketsReceived + 1,
              ),
            );
          } else {
            debugPrint('ConnectionService: Discarding keyframe: missing SPS/PPS.');
            _droppedFramesCount++;
            sendKeyframeRequest();
            return;
          }
        }

        decoderBridge?.feedAccessUnit(
          packet.payload,
          timestampUs: packet.timestampUs,
          isKeyframe: true,
        );

        _recordFrameArrival(now);

        _updateState(
          _state.copyWith(
            status: ConnectionStatus.streaming,
            activeProtocol: StreamingProtocol.ss02H264,
            hasReceivedFirstKeyframe: true,
            lastFrameSizeBytes: packet.payloadLength,
            totalFramesReceived: _state.totalFramesReceived + 1,
            totalBytesReceived: totalBytes,
            keyframePacketsReceived: _state.keyframePacketsReceived + 1,
            hostTimestampMs: packet.timestampUs ~/ 1000,
            clearError: true,
          ),
        );
        break;

      case Ss02PacketType.delta:
        if (!_state.hasCodecConfig || !_state.hasReceivedFirstKeyframe) {
          // Cannot decode delta frames before configuration and an IDR keyframe
          _droppedFramesCount++;
          sendKeyframeRequest();
          return;
        }

        decoderBridge?.feedAccessUnit(
          packet.payload,
          timestampUs: packet.timestampUs,
          isKeyframe: false,
        );

        _recordFrameArrival(now);

        _updateState(
          _state.copyWith(
            status: ConnectionStatus.streaming,
            activeProtocol: StreamingProtocol.ss02H264,
            lastFrameSizeBytes: packet.payloadLength,
            totalFramesReceived: _state.totalFramesReceived + 1,
            totalBytesReceived: totalBytes,
            deltaPacketsReceived: _state.deltaPacketsReceived + 1,
            hostTimestampMs: packet.timestampUs ~/ 1000,
            clearError: true,
          ),
        );
        break;

      case Ss02PacketType.shutdown:
        debugPrint(
          'ConnectionService: Received shutdown notice from Windows host.',
        );
        disconnect(reason: 'Host closed stream gracefully.');
        break;

      case Ss02PacketType.heartbeat:
        // Optional echo back if needed
        break;

      default:
        break;
    }
  }

  void _onSs01FrameReceived(Uint8List frameBytes, int timestampMs) {
    _latestFrame.value = frameBytes;

    final now = DateTime.now().millisecondsSinceEpoch;
    _recordFrameArrival(now);

    final totalBytes =
        _state.totalBytesReceived +
        FrameAccumulator.ss01HeaderSize +
        frameBytes.length;

    _updateState(
      _state.copyWith(
        status: ConnectionStatus.streaming,
        activeProtocol: StreamingProtocol.ss01Jpeg,
        lastFrameSizeBytes: frameBytes.length,
        totalFramesReceived: _state.totalFramesReceived + 1,
        totalBytesReceived: totalBytes,
        hostTimestampMs: timestampMs,
        clearError: true,
      ),
    );
  }

  void _recordFrameArrival(int nowMs) {
    _recentFrameTimestamps.add(nowMs);
    _recentFrameTimestamps.removeWhere((ts) => nowMs - ts > 1000);
    _state = _state.copyWith(receivedFps: _recentFrameTimestamps.length);
  }

  @override
  Future<bool> sendKeyframeRequest() async {
    final socket = _activeSocket;
    if (socket == null) return false;

    // Rate limit: at most 1 keyframe request per 1000ms to prevent flooding
    final now = DateTime.now();
    if (now.difference(_lastKeyframeRequestTime).inMilliseconds < 1000) {
      return false;
    }
    _lastKeyframeRequestTime = now;

    try {
      final reqPacket = createKeyframeRequestPacket(
        sequenceNumber: ++_upstreamSeq,
        timestampUs: now.microsecondsSinceEpoch,
      );

      socket.add(reqPacket);
      await socket.flush();

      _updateState(
        _state.copyWith(
          keyframeRequestsSent: _state.keyframeRequestsSent + 1,
          totalBytesTransmitted:
              _state.totalBytesTransmitted + reqPacket.length,
        ),
      );

      debugPrint('ConnectionService: Sent upstream PACKET_TYPE_KEYFRAME_REQ');
      return true;
    } catch (e) {
      debugPrint('ConnectionService: Failed to send keyframe request: $e');
      return false;
    }
  }

  /// Sends a 44-byte telemetry feedback report (PACKET_TYPE_RECEIVER_REPORT 0x0008)
  /// to the host to inform adaptive bitrate and framerate control.
  @override
  Future<bool> sendReceiverReport({
    int? receivedFps,
    int? decodedFps,
    int? droppedFrames,
    int? bufferDelayMs,
  }) async {
    final socket = _activeSocket;
    if (socket == null ||
        _state.status != ConnectionStatus.streaming ||
        _state.activeProtocol != StreamingProtocol.ss02H264) {
      return false;
    }

    try {
      final now = DateTime.now();
      final rFps = receivedFps ?? _state.receivedFps;
      final dFps = decodedFps ?? _state.receivedFps;
      final drops = droppedFrames ?? _droppedFramesCount;
      final delay = bufferDelayMs ?? (_state.pingMs ?? 0);

      final reportPacket = createReceiverReportPacket(
        sequenceNumber: ++_upstreamSeq,
        timestampUs: now.microsecondsSinceEpoch,
        receivedFps: rFps,
        decodedFps: dFps,
        droppedFrames: drops,
        bufferDelayMs: delay,
      );

      socket.add(reportPacket);
      await socket.flush();

      _updateState(
        _state.copyWith(
          totalBytesTransmitted:
              _state.totalBytesTransmitted + reportPacket.length,
        ),
      );
      return true;
    } catch (e) {
      debugPrint('ConnectionService: Failed to send receiver report: $e');
      return false;
    }
  }

  @override
  Future<bool> sendPointerEvent({
    required PointerEventType type,
    required double normalizedX,
    required double normalizedY,
    PointerButton button = PointerButton.none,
    int modifiers = 0,
    int flags = 0,
  }) async {
    final socket = _activeSocket;
    if (socket == null ||
        _state.status != ConnectionStatus.streaming ||
        _state.activeProtocol != StreamingProtocol.ss02H264) {
      return false;
    }

    try {
      final now = DateTime.now();
      final packet = createPointerEventPacket(
        sequenceNumber: ++_upstreamSeq,
        timestampUs: now.microsecondsSinceEpoch,
        type: type,
        button: button,
        modifiers: modifiers,
        normalizedX: normalizedX,
        normalizedY: normalizedY,
        flags: flags,
      );

      socket.add(packet);
      await socket.flush();

      _updateState(
        _state.copyWith(
          totalBytesTransmitted: _state.totalBytesTransmitted + packet.length,
        ),
      );
      return true;
    } catch (e) {
      debugPrint('ConnectionService: Failed to send pointer event: $e');
      return false;
    }
  }

  @override
  Future<bool> sendScrollEvent({
    required double normalizedX,
    required double normalizedY,
    int deltaX = 0,
    required int deltaY,
  }) async {
    final socket = _activeSocket;
    if (socket == null ||
        _state.status != ConnectionStatus.streaming ||
        _state.activeProtocol != StreamingProtocol.ss02H264) {
      return false;
    }

    try {
      final now = DateTime.now();
      final packet = createScrollEventPacket(
        sequenceNumber: ++_upstreamSeq,
        timestampUs: now.microsecondsSinceEpoch,
        normalizedX: normalizedX,
        normalizedY: normalizedY,
        deltaX: deltaX,
        deltaY: deltaY,
      );

      socket.add(packet);
      await socket.flush();

      _updateState(
        _state.copyWith(
          totalBytesTransmitted: _state.totalBytesTransmitted + packet.length,
        ),
      );
      return true;
    } catch (e) {
      debugPrint('ConnectionService: Failed to send scroll event: $e');
      return false;
    }
  }

  @override
  Future<bool> sendKeyboardEvent({
    required KeyEventType type,
    int keyCode = 0,
    int unicode = 0,
    int modifiers = 0,
  }) async {
    final socket = _activeSocket;
    if (socket == null ||
        _state.status != ConnectionStatus.streaming ||
        _state.activeProtocol != StreamingProtocol.ss02H264) {
      return false;
    }

    try {
      final now = DateTime.now();
      final packet = createKeyboardEventPacket(
        sequenceNumber: ++_upstreamSeq,
        timestampUs: now.microsecondsSinceEpoch,
        type: type,
        modifiers: modifiers,
        keyCode: keyCode,
        unicodeCodepoint: unicode,
      );

      socket.add(packet);
      await socket.flush();

      _updateState(
        _state.copyWith(
          totalBytesTransmitted: _state.totalBytesTransmitted + packet.length,
        ),
      );
      return true;
    } catch (e) {
      debugPrint('ConnectionService: Failed to send keyboard event: $e');
      return false;
    }
  }

  @override
  void cancelConnection() {
    _isCancelled = true;
    _isReconnecting = false;
    _cancelInitialKeyframeRecoveryTimer();
    if (_state.status == ConnectionStatus.connecting) {
      _updateState(
        const ConnectionStateModel(
          status: ConnectionStatus.disconnected,
          errorMessage: 'Connection attempt cancelled.',
        ),
      );
    }
  }

  @override
  Future<bool> reconnect({
    int maxAttempts = 5,
    Duration initialDelay = const Duration(milliseconds: 500),
  }) async {
    if (_isReconnecting) {
      debugPrint('ConnectionService: Reconnection already in progress.');
      return false;
    }

    final target = _lastConnectedDevice;
    if (target == null) {
      debugPrint('ConnectionService: No previous device to reconnect to.');
      return false;
    }

    if (_state.isConnected || _state.isStreaming) {
      debugPrint('ConnectionService: Already connected.');
      return true;
    }

    _isReconnecting = true;
    _isCancelled = false;

    debugPrint(
      'ConnectionService: Starting automatic reconnection to ${target.address}:${target.port} (max $maxAttempts attempts)...',
    );

    try {
      for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        if (_isCancelled || _isDisposed) {
          debugPrint('ConnectionService: Reconnection aborted.');
          return false;
        }

        if (attempt > 1) {
          // Bounded exponential backoff with deterministic jitter
          final backoffMs = (initialDelay.inMilliseconds * (1 << (attempt - 2)))
              .clamp(initialDelay.inMilliseconds, 5000);
          final jitterMs = (attempt * 37) % 250;
          final totalDelay = Duration(milliseconds: backoffMs + jitterMs);

          debugPrint(
            'ConnectionService: Reconnection attempt $attempt/$maxAttempts in ${totalDelay.inMilliseconds}ms...',
          );
          await Future.delayed(totalDelay);
        }

        if (_isCancelled || _isDisposed) return false;

        final success = await connectToDevice(
          target.address,
          target.port,
          pcName: target.name,
          isUsb: _lastConnectWasUsb,
        );

        if (success) {
          debugPrint(
            'ConnectionService: Reconnected successfully on attempt $attempt.',
          );
          return true;
        }
      }

      debugPrint(
        'ConnectionService: All $maxAttempts reconnection attempts failed.',
      );
      return false;
    } finally {
      _isReconnecting = false;
    }
  }

  @override
  Future<void> disconnect({String? reason}) async {
    _isClosing = true;
    _isReconnecting = false;
    _cancelInitialKeyframeRecoveryTimer();
    await _closeSocket();
    if (!_isDisposed) {
      await decoderBridge?.resetDecoder();
    }
    _accumulator?.reset();
    _accumulator = null;
    _recentFrameTimestamps.clear();
    _latestFrame.value = null;

    if (!_isDisposed) {
      _updateState(
        ConnectionStateModel(
          status: reason != null
              ? ConnectionStatus.failed
              : ConnectionStatus.disconnected,
          errorMessage: reason,
          connectedDevice: reason != null ? _lastConnectedDevice : null,
        ),
      );
    }
    _isClosing = false;
    debugPrint(
      'ConnectionService: Disconnected. Reason: ${reason ?? "User disconnected"}',
    );
  }

  Future<void> _closeSocket() async {
    _receiverReportTimer?.cancel();
    _receiverReportTimer = null;
    if (_activeSocket != null) {
      try {
        await _activeSocket?.close();
        _activeSocket?.destroy();
      } catch (_) {}
      _activeSocket = null;
    }
  }

  void _updateState(ConnectionStateModel newState) {
    if (_isDisposed) return;
    _state = newState;
    notifyListeners();
  }

  @override
  void dispose() {
    _isDisposed = true;
    _keyframeRequestSubscription?.cancel();
    _closeSocket();
    _latestFrame.dispose();
    super.dispose();
  }
}
