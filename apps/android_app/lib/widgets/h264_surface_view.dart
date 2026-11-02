import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/decoder_bridge.dart';

/// Embedded native Android SurfaceView for zero-copy Direct3D/MediaCodec video display.
class H264SurfaceView extends StatelessWidget {
  static const String viewType = 'com.secondscreen/video_surface';

  final SecondScreenDecoderBridge bridge;
  final double aspectRatio;
  final bool showDebugBadge;

  const H264SurfaceView({
    super.key,
    required this.bridge,
    this.aspectRatio = 16 / 9,
    this.showDebugBadge = true,
  });

  @override
  Widget build(BuildContext context) {
    return Center(
      child: AspectRatio(
        aspectRatio: aspectRatio,
        child: Stack(
          fit: StackFit.expand,
          children: [
            // 1. Native SurfaceView Platform View (Android) or Mock Fallback (Test/Desktop)
            _buildNativePlatformView(),

            // 2. State & Loading Overlay
            ListenableBuilder(
              listenable: bridge,
              builder: (context, _) {
                final state = bridge.currentState;
                if (state == 'DECODING') {
                  return const SizedBox.shrink();
                }

                return Container(
                  color: Colors.black.withValues(alpha: 0.6),
                  alignment: Alignment.center,
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const CircularProgressIndicator(color: Colors.white),
                      const SizedBox(height: 16),
                      Text(
                        _getLabelForState(state),
                        style: const TextStyle(
                          color: Colors.white,
                          fontSize: 14,
                          fontWeight: FontWeight.w500,
                        ),
                      ),
                    ],
                  ),
                );
              },
            ),

            // 3. Optional Debug Badge
            if (showDebugBadge)
              Positioned(
                top: 8,
                left: 8,
                child: ListenableBuilder(
                  listenable: bridge,
                  builder: (context, _) {
                    return Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 8,
                        vertical: 4,
                      ),
                      decoration: BoxDecoration(
                        color: Colors.black.withValues(alpha: 0.7),
                        borderRadius: BorderRadius.circular(4),
                        border: Border.all(color: Colors.white24),
                      ),
                      child: Text(
                        'H.264 MediaCodec: ${bridge.currentState}',
                        style: const TextStyle(
                          color: Colors.greenAccent,
                          fontSize: 11,
                          fontFamily: 'monospace',
                        ),
                      ),
                    );
                  },
                ),
              ),
          ],
        ),
      ),
    );
  }

  Widget _buildNativePlatformView() {
    if (defaultTargetPlatform == TargetPlatform.android) {
      return const AndroidView(
        viewType: viewType,
        creationParams: <String, dynamic>{},
        creationParamsCodec: StandardMessageCodec(),
      );
    } else {
      // Non-Android fallback for tests and desktop runner
      return Container(
        color: const Color(0xFF0D1117),
        child: const Center(
          child: Text(
            'H.264 Native SurfaceView (Android Platform View)',
            style: TextStyle(color: Colors.white38, fontSize: 13),
          ),
        ),
      );
    }
  }

  String _getLabelForState(String state) {
    switch (state) {
      case 'INITIALIZING':
        return 'Initializing MediaCodec Hardware Decoder...';
      case 'WAITING_FOR_CONFIG':
        return 'Awaiting SPS/PPS Codec Configuration...';
      case 'WAITING_FOR_KEYFRAME':
        return 'Awaiting IDR Keyframe...';
      case 'FLUSHING':
        return 'Flushing Video Pipeline...';
      case 'ERROR':
        return 'Decoder Encountered an Error';
      default:
        return 'Preparing Secondary Display...';
    }
  }
}
