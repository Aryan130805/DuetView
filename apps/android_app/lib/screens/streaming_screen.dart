import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/connection_state.dart';
import '../services/connection_service.dart';
import '../services/settings_service.dart';
import '../theme/app_theme.dart';

/// Dedicated viewport screen for displaying the secondary monitor stream.
///
/// In Phase 3, displays real-time desktop JPEG frames received from the
/// Windows streaming host via the SecondScreen v1 TCP protocol.
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

  Future<void> _handleRetry() async {
    final dev = widget.connectionService.state.connectedDevice;
    final lastHost = dev?.address.isNotEmpty == true
        ? dev!.address
        : widget.settingsService.settings.lastHostAddress;
    final port = dev?.port ?? widget.settingsService.settings.defaultPort;

    if (lastHost.isNotEmpty) {
      await widget.connectionService.connectToDevice(lastHost, port);
    } else {
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
                  top: _showOverlay ? 0 : -95,
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
    // 1. If actively streaming, render the incoming JPEG frames
    if (state.isStreaming) {
      return ValueListenableBuilder<Uint8List?>(
        valueListenable: widget.connectionService.latestFrame,
        builder: (context, frameBytes, _) {
          if (frameBytes != null && frameBytes.isNotEmpty) {
            return Image.memory(
              frameBytes,
              gaplessPlayback: true,
              fit: BoxFit.contain,
              width: double.infinity,
              height: double.infinity,
              errorBuilder: (context, error, stackTrace) {
                return _buildErrorState('Error decoding frame: $error');
              },
            );
          }
          return _buildWaitingState(state);
        },
      );
    }

    // 2. Connecting state
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

    // 3. Connected but awaiting the first frame
    if (state.isConnected) {
      return _buildWaitingState(state);
    }

    // 4. Failed or Disconnected state
    return _buildErrorState(state.errorMessage ?? 'Disconnected from host');
  }

  Widget _buildWaitingState(ConnectionStateModel state) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const SizedBox(
            width: 32,
            height: 32,
            child: CircularProgressIndicator(
              strokeWidth: 3,
              color: Color(0xFF38BDF8),
            ),
          ),
          const SizedBox(height: 20),
          Text(
            'Connected to ${state.connectedDevice?.address ?? "Windows PC"}',
            style: const TextStyle(
              color: Colors.white,
              fontSize: 16,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 8),
          const Text(
            'Awaiting initial desktop frame transmission...',
            style: TextStyle(color: Colors.white70, fontSize: 13),
          ),
        ],
      ),
    );
  }

  Widget _buildErrorState(String message) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.link_off, color: Colors.white54, size: 48),
            const SizedBox(height: 16),
            Text(
              message,
              style: const TextStyle(color: Colors.white, fontSize: 16),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 20),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                OutlinedButton(
                  onPressed: () => Navigator.pop(context),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: Colors.white70,
                    side: const BorderSide(color: Colors.white38),
                  ),
                  child: const Text('Back to Home'),
                ),
                const SizedBox(width: 12),
                FilledButton.icon(
                  onPressed: _handleRetry,
                  icon: const Icon(Icons.refresh, size: 18),
                  label: const Text('Retry Connection'),
                ),
              ],
            ),
          ],
        ),
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
                  _buildMetricsString(state, settings),
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

  String _buildMetricsString(ConnectionStateModel state, dynamic settings) {
    if (state.isStreaming) {
      final frameKb = (state.lastFrameSizeBytes / 1024).toStringAsFixed(1);
      final transitStr = state.pingMs != null ? ' • ${state.pingMs} ms' : '';
      return '${state.receivedFps} FPS • $frameKb KB/frame$transitStr';
    }
    return '${settings.resolution.label} • ${settings.frameRate.fps} FPS'
        '${state.pingMs != null ? " • ${state.pingMs} ms" : ""}';
  }
}
