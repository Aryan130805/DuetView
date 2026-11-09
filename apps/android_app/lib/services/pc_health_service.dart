import 'dart:async';
import 'dart:io';

import 'package:flutter/widgets.dart';

import '../models/saved_pc.dart';
import 'frame_accumulator.dart';
import 'saved_pc_storage.dart';

/// Periodically performs lightweight health checks on saved host PCs
/// to update their online/offline indicators without opening video streams.
class PcHealthService with WidgetsBindingObserver {
  final SavedPcStorage storage;
  final Duration checkInterval;
  final int maxMissedChecks;

  Timer? _timer;
  bool _isChecking = false;
  bool _isPaused = false;
  final Map<String, int> _consecutiveMisses = {};
  String? _activelyStreamingPcId;
  String? _activelyStreamingHost;

  PcHealthService({
    required this.storage,
    this.checkInterval = const Duration(seconds: 5),
    this.maxMissedChecks = 3,
  }) {
    WidgetsBinding.instance.addObserver(this);
  }

  void start() {
    _timer?.cancel();
    _timer = Timer.periodic(checkInterval, (_) {
      checkAllNow();
    });
    // Run an immediate initial check
    checkAllNow();
  }

  void stop() {
    _timer?.cancel();
    _timer = null;
  }

  void pauseHealthChecks() {
    _isPaused = true;
  }

  void resumeHealthChecks() {
    _isPaused = false;
  }

  void setActivelyStreamingPc(String? pcId, {String? host}) {
    _activelyStreamingPcId = pcId;
    _activelyStreamingHost = host;
    if (pcId != null) {
      storage.updatePcStatus(pcId, PcOnlineStatus.streaming);
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      start();
    } else if (state == AppLifecycleState.paused || state == AppLifecycleState.inactive) {
      stop();
    }
  }

  Future<void> checkAllNow() async {
    if (_isChecking || _isPaused) return;
    _isChecking = true;

    try {
      final pcs = List<SavedPc>.from(storage.savedPcs);
      for (final pc in pcs) {
        if (pc.id == _activelyStreamingPcId) {
          continue; // Do not interrupt active stream with health checks
        }
        if (_activelyStreamingHost != null && pc.endpoints.contains(_activelyStreamingHost)) {
          continue; // Skip any PC matching the actively connected IP
        }
        await _checkSinglePc(pc);
      }
    } finally {
      _isChecking = false;
    }
  }

  Future<void> _checkSinglePc(SavedPc pc) async {
    final sw = Stopwatch()..start();
    String? successfulEndpoint;
    bool isReachable = false;

    for (final ep in pc.endpoints) {
      if (await _pingEndpoint(ep, pc.port)) {
        isReachable = true;
        successfulEndpoint = ep;
        break;
      }
    }

    sw.stop();

    if (isReachable) {
      _consecutiveMisses[pc.id] = 0;
      await storage.updatePcStatus(
        pc.id,
        PcOnlineStatus.online,
        pingMs: sw.elapsedMilliseconds,
        reachableEndpoint: successfulEndpoint,
      );
    } else {
      final misses = (_consecutiveMisses[pc.id] ?? 0) + 1;
      _consecutiveMisses[pc.id] = misses;

      if (misses >= maxMissedChecks) {
        await storage.updatePcStatus(pc.id, PcOnlineStatus.offline);
      }
    }
  }

  Future<bool> _pingEndpoint(String host, int port) async {
    Socket? socket;
    try {
      socket = await Socket.connect(
        host,
        port,
        timeout: const Duration(milliseconds: 1500),
      );

      final completer = Completer<bool>();
      final accumulator = FrameAccumulator(
        onSs02Packet: (packet) {
          if (packet.type == Ss02PacketType.heartbeatAck) {
            if (!completer.isCompleted) completer.complete(true);
          }
        },
        onFrame: (frame, timestamp) {},
        onError: (_) {},
      );

      socket.listen(
        (data) => accumulator.addChunk(data),
        onError: (_) {
          if (!completer.isCompleted) completer.complete(false);
        },
        cancelOnError: true,
      );

      // Send SS02 Heartbeat packet
      final hb = createHeartbeatPacket(
        sequenceNumber: 1,
        timestampUs: DateTime.now().microsecondsSinceEpoch,
      );
      socket.add(hb);
      await socket.flush();

      // Acknowledge only if genuine SS02 heartbeatAck packet arrives from host
      final ackReceived = await completer.future.timeout(
        const Duration(milliseconds: 1000),
        onTimeout: () => false,
      );

      return ackReceived;
    } catch (_) {
      return false;
    } finally {
      try {
        socket?.destroy();
      } catch (_) {}
    }
  }

  void dispose() {
    stop();
    WidgetsBinding.instance.removeObserver(this);
  }
}

