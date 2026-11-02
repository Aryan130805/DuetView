import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Capabilities of the native Android H.264 decoder.
class DecoderCapabilities {
  final String codecName;
  final bool isHardwareAccelerated;
  final int maxWidth;
  final int maxHeight;
  final bool isSupported;

  const DecoderCapabilities({
    required this.codecName,
    required this.isHardwareAccelerated,
    required this.maxWidth,
    required this.maxHeight,
    required this.isSupported,
  });

  factory DecoderCapabilities.fromMap(Map<dynamic, dynamic> map) {
    return DecoderCapabilities(
      codecName: map['codecName'] as String? ?? 'Unknown',
      isHardwareAccelerated: map['isHardwareAccelerated'] as bool? ?? false,
      maxWidth: map['maxWidth'] as int? ?? 0,
      maxHeight: map['maxHeight'] as int? ?? 0,
      isSupported: map['isSupported'] as bool? ?? false,
    );
  }
}

/// Real-time metrics reported by the native H.264 decoder.
class DecoderMetrics {
  final String codecName;
  final bool isHardwareAccelerated;
  final String state;
  final int configuredWidth;
  final int configuredHeight;
  final int submittedFrames;
  final int decodedFrames;
  final double outputFps;
  final int decodeErrors;
  final int keyframeWaitCount;

  const DecoderMetrics({
    required this.codecName,
    required this.isHardwareAccelerated,
    required this.state,
    required this.configuredWidth,
    required this.configuredHeight,
    required this.submittedFrames,
    required this.decodedFrames,
    required this.outputFps,
    required this.decodeErrors,
    required this.keyframeWaitCount,
  });

  factory DecoderMetrics.fromMap(Map<dynamic, dynamic> map) {
    return DecoderMetrics(
      codecName: map['codecName'] as String? ?? 'Unknown',
      isHardwareAccelerated: map['isHardwareAccelerated'] as bool? ?? false,
      state: map['state'] as String? ?? 'UNINITIALIZED',
      configuredWidth: map['configuredWidth'] as int? ?? 0,
      configuredHeight: map['configuredHeight'] as int? ?? 0,
      submittedFrames: (map['submittedFrames'] as num?)?.toInt() ?? 0,
      decodedFrames: (map['decodedFrames'] as num?)?.toInt() ?? 0,
      outputFps: (map['outputFps'] as num?)?.toDouble() ?? 0.0,
      decodeErrors: (map['decodeErrors'] as num?)?.toInt() ?? 0,
      keyframeWaitCount: (map['keyframeWaitCount'] as num?)?.toInt() ?? 0,
    );
  }
}

/// Service bridging Flutter with the native Android MediaCodec H.264 decoder.
class SecondScreenDecoderBridge extends ChangeNotifier {
  static const MethodChannel _channel =
      MethodChannel('com.secondscreen/decoder_control');

  final StreamController<String> _stateController =
      StreamController<String>.broadcast();
  final StreamController<void> _keyframeRequestController =
      StreamController<void>.broadcast();
  final StreamController<String> _errorController =
      StreamController<String>.broadcast();

  String _currentState = 'UNINITIALIZED';
  String get currentState => _currentState;

  Stream<String> get onStateChanged => _stateController.stream;
  Stream<void> get onKeyframeRequested => _keyframeRequestController.stream;
  Stream<String> get onDecoderError => _errorController.stream;

  SecondScreenDecoderBridge() {
    _channel.setMethodCallHandler(_handleNativeCall);
  }

  Future<void> _handleNativeCall(MethodCall call) async {
    switch (call.method) {
      case 'onStateChanged':
        final state = call.arguments['state'] as String? ?? 'UNKNOWN';
        _currentState = state;
        _stateController.add(state);
        notifyListeners();
        break;
      case 'onKeyframeRequested':
        _keyframeRequestController.add(null);
        break;
      case 'onDecoderError':
        final err = call.arguments['error'] as String? ?? 'Unknown decoder error';
        _errorController.add(err);
        break;
    }
  }

  /// Probes available H.264 decoders on the device.
  Future<DecoderCapabilities> getDecoderCapabilities() async {
    try {
      final res = await _channel.invokeMethod<Map<dynamic, dynamic>>(
          'getDecoderCapabilities');
      if (res != null) {
        return DecoderCapabilities.fromMap(res);
      }
    } on MissingPluginException {
      // Mock for desktop / test environments
      return const DecoderCapabilities(
        codecName: 'MockH264Decoder',
        isHardwareAccelerated: false,
        maxWidth: 1920,
        maxHeight: 1080,
        isSupported: true,
      );
    } catch (e) {
      debugPrint('Error getting decoder capabilities: $e');
    }
    return const DecoderCapabilities(
      codecName: 'None',
      isHardwareAccelerated: false,
      maxWidth: 0,
      maxHeight: 0,
      isSupported: false,
    );
  }

  /// Initializes the native decoder for the given display resolution.
  Future<bool> initializeDecoder({int width = 1280, int height = 720}) async {
    try {
      final res = await _channel.invokeMethod<bool>('initializeDecoder', {
        'width': width,
        'height': height,
      });
      return res ?? false;
    } on MissingPluginException {
      _currentState = 'WAITING_FOR_CONFIG';
      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Error initializing decoder: $e');
      return false;
    }
  }

  /// Ingests SS02 PACKET_TYPE_CONFIG payload (SPS/PPS).
  Future<bool> feedConfigPacket(Uint8List data) async {
    try {
      final res = await _channel.invokeMethod<bool>('feedConfigPacket', {
        'data': data,
      });
      return res ?? false;
    } on MissingPluginException {
      _currentState = 'WAITING_FOR_KEYFRAME';
      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Error feeding config packet: $e');
      return false;
    }
  }

  /// Ingests an encoded H.264 access unit (KEYFRAME or DELTA).
  Future<bool> feedAccessUnit(
    Uint8List data, {
    int timestampUs = 0,
    bool isKeyframe = false,
  }) async {
    try {
      final res = await _channel.invokeMethod<bool>('feedAccessUnit', {
        'data': data,
        'timestampUs': timestampUs,
        'isKeyframe': isKeyframe,
      });
      return res ?? false;
    } on MissingPluginException {
      _currentState = 'DECODING';
      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Error feeding access unit: $e');
      return false;
    }
  }

  /// Resets the decoder.
  Future<bool> resetDecoder() async {
    try {
      final res = await _channel.invokeMethod<bool>('resetDecoder');
      return res ?? false;
    } on MissingPluginException {
      _currentState = 'UNINITIALIZED';
      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Error resetting decoder: $e');
      return false;
    }
  }

  /// Sets or clears the Android window FLAG_KEEP_SCREEN_ON.
  Future<bool> setKeepScreenOn(bool enable) async {
    try {
      final res = await _channel.invokeMethod<bool>('setKeepScreenOn', {
        'enable': enable,
      });
      return res ?? true;
    } on MissingPluginException {
      return true;
    } catch (e) {
      debugPrint('Error setting keep screen on: $e');
      return false;
    }
  }

  /// Fetches real-time performance and error metrics from the decoder.
  Future<DecoderMetrics> getMetrics() async {
    try {
      final res =
          await _channel.invokeMethod<Map<dynamic, dynamic>>('getMetrics');
      if (res != null) {
        return DecoderMetrics.fromMap(res);
      }
    } on MissingPluginException {
      return DecoderMetrics(
        codecName: 'MockH264Decoder',
        isHardwareAccelerated: false,
        state: _currentState,
        configuredWidth: 1280,
        configuredHeight: 720,
        submittedFrames: 0,
        decodedFrames: 0,
        outputFps: 0.0,
        decodeErrors: 0,
        keyframeWaitCount: 0,
      );
    } catch (e) {
      debugPrint('Error fetching decoder metrics: $e');
    }
    return DecoderMetrics(
      codecName: 'Unknown',
      isHardwareAccelerated: false,
      state: _currentState,
      configuredWidth: 0,
      configuredHeight: 0,
      submittedFrames: 0,
      decodedFrames: 0,
      outputFps: 0.0,
      decodeErrors: 0,
      keyframeWaitCount: 0,
    );
  }

  /// Executes an isolated native H.264 decoder smoke test.
  Future<Map<String, dynamic>> runSmokeTest({
    int width = 1280,
    int height = 720,
    int frames = 15,
  }) async {
    try {
      final res = await _channel.invokeMethod<Map<dynamic, dynamic>>(
        'runSmokeTest',
        {
          'width': width,
          'height': height,
          'frames': frames,
        },
      );
      if (res != null) {
        return res.cast<String, dynamic>();
      }
    } on MissingPluginException {
      return {
        'success': true,
        'codecName': 'MockH264Decoder',
        'isHardwareAccelerated': false,
        'submittedFrames': frames,
        'decodedFrames': frames,
        'decodeErrors': 0,
        'state': 'DECODING',
      };
    } catch (e) {
      debugPrint('Error running smoke test: $e');
    }
    return {
      'success': false,
      'error': 'Bridge call failed',
    };
  }

  /// Opens Android system USB Tethering settings directly.
  Future<bool> openTetheringSettings() async {
    try {
      final res = await _channel.invokeMethod<bool>('openTetheringSettings');
      return res ?? false;
    } on MissingPluginException {
      return false;
    } catch (e) {
      debugPrint('Error opening tethering settings: $e');
      return false;
    }
  }

  @override
  void dispose() {
    _stateController.close();
    _keyframeRequestController.close();
    _errorController.close();
    super.dispose();
  }
}
