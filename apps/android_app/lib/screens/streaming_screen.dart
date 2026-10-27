import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/connection_state.dart';
import '../services/connection_service.dart';
import '../services/settings_service.dart';
import '../theme/app_theme.dart';

/// Dedicated viewport screen for displaying the secondary monitor stream.
///
/// Designed so that in Phases 3 and 4, the [StreamViewportPlaceholder] can be
/// swapped with the native [Texture] / [SurfaceView] hardware video decoder
/// without altering the container, controls, or aspect-ratio scaling.
class StreamingScreen extends StatefulWidget {
  final ConnectionService connectionService;
  final SettingsService settingsService;

  const StreamingScreen({
    super.key,
    required this.connectionService,
    required this.settingsService,
  });

  @override
  State<StreamingScreen> createState() => _StreamingScreenState();
}

class _StreamingScreenState extends State<StreamingScreen> {
  bool _showOverlay = true;

  @override
  void initState() {
    super.initState();
    if (widget.settingsService.settings.fullScreen) {
      SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    }
  }

  @override
  void dispose() {
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
    super.dispose();
  }

  void _toggleOverlay() {
    setState(() {
      _showOverlay = !_showOverlay;
    });
  }

  Future<void> _handleDisconnect() async {
    await widget.connectionService.disconnect();
    if (mounted) {
      Navigator.pop(context);
    }
  }

  @override
  Widget build(BuildContext context) {
    final settings = widget.settingsService.settings;
    final aspectRatio = settings.resolution.width / settings.resolution.height;

    return Scaffold(
      backgroundColor: Colors.black,
      body: ListenableBuilder(
        listenable: widget.connectionService,
        builder: (context, _) {
          final state = widget.connectionService.state;

          return GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTap: _toggleOverlay,
            child: Stack(
              children: [
                // 1. Center Display Viewport with preserved Aspect Ratio
                Center(
                  child: AspectRatio(
                    aspectRatio: aspectRatio,
                    child: Container(
                      color: const Color(0xFF090D16),
                      child: _buildViewportContent(state),
                    ),
                  ),
                ),

                // 2. Status Overlay Bar (top)
                AnimatedPositioned(
                  duration: const Duration(milliseconds: 250),
                  curve: Curves.easeInOut,
                  top: _showOverlay ? 0 : -90,
                  left: 0,
                  right: 0,
                  child: _buildTopOverlay(context, state),
                ),

                // 3. Bottom Tap Hint when overlay is hidden
                if (!_showOverlay)
                  Positioned(
                    bottom: 16,
                    left: 0,
                    right: 0,
                    child: Center(
                      child: Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 16,
                          vertical: 8,
                        ),
                        decoration: BoxDecoration(
                          color: Colors.black.withValues(alpha: 0.6),
                          borderRadius: BorderRadius.circular(20),
                          border: Border.all(color: Colors.white24),
                        ),
                        child: const Text(
                          'Tap screen to show controls',
                          style: TextStyle(color: Colors.white70, fontSize: 12),
                        ),
                      ),
                    ),
                  ),
              ],
            ),
          );
        },
      ),
    );
  }

  Widget _buildViewportContent(ConnectionStateModel state) {
    if (state.isConnecting) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const CircularProgressIndicator(color: Colors.white),
            const SizedBox(height: 20),
            Text(
              'Connecting to ${state.connectedDevice?.address ?? "PC"}...',
              style: const TextStyle(color: Colors.white, fontSize: 16),
            ),
            const SizedBox(height: 8),
            const Text(
              'Establishing low-latency TCP connection',
              style: TextStyle(color: Colors.white54, fontSize: 13),
            ),
          ],
        ),
      );
    }

    if (state.isConnected) {
      // Architectural Placeholder: Clearly explains state without fake streaming
      final settings = widget.settingsService.settings;
      return StreamViewportPlaceholder(
        device: state.connectedDevice,
        resolutionLabel: settings.resolution.label,
        fps: settings.frameRate.fps,
        pingMs: state.pingMs,
      );
    }

    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.link_off, color: Colors.white54, size: 48),
          const SizedBox(height: 16),
          Text(
            state.errorMessage ?? 'Disconnected from PC',
            style: const TextStyle(color: Colors.white, fontSize: 16),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 16),
          FilledButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Back to Home'),
          ),
        ],
      ),
    );
  }

  Widget _buildTopOverlay(BuildContext context, ConnectionStateModel state) {
    final statusColor = AppTheme.colorForStatus(state.status);
    final settings = widget.settingsService.settings;

    return Container(
      padding: EdgeInsets.only(
        top: MediaQuery.of(context).padding.top + 8,
        bottom: 12,
        left: 16,
        right: 16,
      ),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [Colors.black.withValues(alpha: 0.85), Colors.transparent],
        ),
      ),
      child: Row(
        children: [
          // Status indicator dot
          Container(
            width: 10,
            height: 10,
            decoration: BoxDecoration(
              color: statusColor,
              shape: BoxShape.circle,
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  state.connectedDevice?.name ?? 'Windows Secondary Display',
                  style: const TextStyle(
                    color: Colors.white,
                    fontWeight: FontWeight.w600,
                    fontSize: 14,
                  ),
                ),
                Text(
                  '${settings.resolution.label} • ${settings.frameRate.fps} FPS'
                  '${state.pingMs != null ? " • ${state.pingMs} ms" : ""}',
                  style: const TextStyle(color: Colors.white70, fontSize: 12),
                ),
              ],
            ),
          ),
          // Disconnect action button
          OutlinedButton.icon(
            onPressed: _handleDisconnect,
            icon: const Icon(Icons.close, size: 16, color: Colors.white),
            label: const Text(
              'Disconnect',
              style: TextStyle(color: Colors.white),
            ),
            style: OutlinedButton.styleFrom(
              side: const BorderSide(color: Colors.white38),
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
            ),
          ),
        ],
      ),
    );
  }
}

/// Placeholder viewport component representing the display canvas.
///
/// This component will be replaced by the hardware video surface in Phase 3 & 4.
class StreamViewportPlaceholder extends StatelessWidget {
  final dynamic device;
  final String resolutionLabel;
  final int fps;
  final int? pingMs;

  const StreamViewportPlaceholder({
    super.key,
    required this.device,
    required this.resolutionLabel,
    required this.fps,
    this.pingMs,
  });

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: const Color(0xFF1E293B),
                borderRadius: BorderRadius.circular(16),
                border: Border.all(color: const Color(0xFF334155)),
              ),
              child: const Icon(
                Icons.connected_tv,
                size: 52,
                color: Color(0xFF38BDF8),
              ),
            ),
            const SizedBox(height: 20),
            const Text(
              'Secondary Display Stream Viewport',
              style: TextStyle(
                color: Colors.white,
                fontSize: 18,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Target: $resolutionLabel @ $fps FPS',
              style: const TextStyle(
                color: Colors.white70,
                fontSize: 14,
                fontWeight: FontWeight.w500,
              ),
            ),
            const SizedBox(height: 16),
            Container(
              constraints: const BoxConstraints(maxWidth: 480),
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.05),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(color: Colors.white12),
              ),
              child: const Text(
                'Phase 2 MVP: TCP connection established. '
                'In Phase 3 and 4, the native Direct3D screen-capture and MediaCodec video pipeline '
                'will stream Windows desktop frames directly onto this canvas.',
                textAlign: TextAlign.center,
                style: TextStyle(
                  color: Colors.white60,
                  fontSize: 12,
                  height: 1.4,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
