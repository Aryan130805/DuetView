import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/connection_state.dart';
import '../services/connection_service.dart';
import '../services/decoder_bridge.dart';
import '../services/settings_service.dart';
import '../theme/app_theme.dart';
import '../widgets/h264_surface_view.dart';
import '../widgets/touch_input_overlay.dart';
import '../widgets/on_screen_keyboard_bar.dart';

/// Dedicated viewport screen for displaying the secondary monitor stream.
///
/// Supports Phase 4 native H.264 SurfaceView hardware rendering as well as
/// Phase 3 real-time desktop JPEG streaming fallback.
class StreamingScreen extends StatefulWidget {
  final ConnectionService connectionService;
  final SettingsService settingsService;
  final SecondScreenDecoderBridge? decoderBridge;

  const StreamingScreen({
    super.key,
    required this.connectionService,
    required this.settingsService,
    this.decoderBridge,
  });

  @override
  State<StreamingScreen> createState() => _StreamingScreenState();
}

class _StreamingScreenState extends State<StreamingScreen>
    with WidgetsBindingObserver {
  bool _showOverlay = true;
  bool _touchInputEnabled = true;
  TouchInputMode _inputMode = TouchInputMode.direct;
  bool _showKeyboard = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);

    // Keep screen awake during active secondary display streaming
    widget.decoderBridge?.setKeepScreenOn(true);

    // Force landscape orientation for display streaming
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    if (widget.settingsService.settings.fullScreen) {
      SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);
    if (state == AppLifecycleState.resumed) {
      // Screen turned on or app returned to foreground
      if (widget.settingsService.settings.fullScreen) {
        SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
      }
      widget.decoderBridge?.setKeepScreenOn(true);

      // Check socket health and recover stream
      if (widget.connectionService.isSocketConnected) {
        // TCP socket is still healthy; request immediate IDR keyframe to populate Surface
        debugPrint(
          'StreamingScreen: Resumed with active socket; requesting immediate keyframe.',
        );
        widget.connectionService.sendKeyframeRequest();
      } else if (!widget.connectionService.isReconnecting &&
          widget.connectionService.lastConnectedDevice != null) {
        // Connection dropped while screen was off; initiate automatic reconnection
        debugPrint(
          'StreamingScreen: Resumed with dropped socket; triggering automatic reconnection.',
        );
        _triggerAutoReconnect();
      }
    } else if (state == AppLifecycleState.paused) {
      debugPrint('StreamingScreen: App paused / screen turned off.');
    }
  }

  void _triggerAutoReconnect() {
    if (!mounted) return;
    widget.connectionService.reconnect().then((success) {
      if (mounted && success) {
        widget.connectionService.sendKeyframeRequest();
      }
    });
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    widget.decoderBridge?.setKeepScreenOn(false);

    // Restore all orientations when exiting streaming screen
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.portraitUp,
      DeviceOrientation.portraitDown,
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
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
    final dev = widget.connectionService.lastConnectedDevice ??
        widget.connectionService.state.connectedDevice;
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

          return Stack(
            children: [
              // 1. Center Display Viewport with Touch Input Overlay
              TouchInputOverlay(
                connectionService: widget.connectionService,
                desktopAspectRatio: aspectRatio,
                enabled: _touchInputEnabled && state.isStreaming,
                inputMode: _inputMode,
                onInputModeChanged: (mode) => setState(() => _inputMode = mode),
                onToggleControls: _toggleOverlay,
                child: Center(
                  child: AspectRatio(
                    aspectRatio: aspectRatio,
                    child: Container(
                      color: const Color(0xFF090D16),
                      child: _buildViewportContent(state),
                    ),
                  ),
                ),
              ),

              // 2. Status Overlay Bar (top)
              AnimatedPositioned(
                duration: const Duration(milliseconds: 250),
                curve: Curves.easeInOut,
                top: _showOverlay ? 0 : -110,
                left: 0,
                right: 0,
                child: _buildTopOverlay(context, state),
              ),

              // 3. Dropdown Menu Trigger ("V" on the left) when overlay is hidden
              if (!_showOverlay)
                Positioned(
                  top: MediaQuery.of(context).padding.top + 8,
                  left: 12,
                  child: Material(
                    color: Colors.transparent,
                    child: InkWell(
                      onTap: _toggleOverlay,
                      borderRadius: BorderRadius.circular(20),
                      child: Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 10,
                          vertical: 6,
                        ),
                        decoration: BoxDecoration(
                          color: Colors.black.withValues(alpha: 0.8),
                          borderRadius: BorderRadius.circular(20),
                          border: Border.all(
                            color: const Color(0xFF38BDF8).withValues(alpha: 0.5),
                          ),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black.withValues(alpha: 0.5),
                              blurRadius: 8,
                            ),
                          ],
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            const Icon(
                              Icons.keyboard_arrow_down,
                              color: Color(0xFF38BDF8),
                              size: 22,
                            ),
                            const SizedBox(width: 6),
                            Container(
                              width: 8,
                              height: 8,
                              decoration: BoxDecoration(
                                color: AppTheme.colorForStatus(state.status),
                                shape: BoxShape.circle,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ),

              // 4. On-Screen Keyboard Accessory Bar
              if (_showKeyboard && state.isStreaming)
                Positioned(
                  left: 0,
                  right: 0,
                  bottom: 0,
                  child: OnScreenKeyboardBar(
                    connectionService: widget.connectionService,
                    onClose: () => setState(() => _showKeyboard = false),
                  ),
                ),
            ],
          );
        },
      ),
    );
  }

  Widget _buildViewportContent(ConnectionStateModel state) {
    // 1. If actively streaming, check whether native H.264 SurfaceView or JPEG fallback is active
    if (state.isStreaming) {
      return ValueListenableBuilder<Uint8List?>(
        valueListenable: widget.connectionService.latestFrame,
        builder: (context, frameBytes, _) {
          // If JPEG frame bytes exist, render via Phase 3 JPEG fallback
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

          // Otherwise, if H.264 decoder bridge is attached, render native SurfaceView
          if (widget.decoderBridge != null) {
            final res = widget.settingsService.settings.resolution;
            return H264SurfaceView(
              bridge: widget.decoderBridge!,
              aspectRatio: res.width / res.height,
              showDebugBadge: false,
            );
          }

          return _buildWaitingState(state);
        },
      );
    }

    // 2. Connecting or Reconnecting state
    if (state.isConnecting || widget.connectionService.isReconnecting) {
      final target = state.connectedDevice ??
          widget.connectionService.lastConnectedDevice;
      final isReconnecting = widget.connectionService.isReconnecting;
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const CircularProgressIndicator(color: Colors.white),
            const SizedBox(height: 20),
            Text(
              isReconnecting
                  ? 'Reconnecting to ${target?.name ?? target?.address ?? "PC"}...'
                  : 'Connecting to ${target?.address ?? "PC"}...',
              style: const TextStyle(color: Colors.white, fontSize: 16),
            ),
            const SizedBox(height: 8),
            Text(
              isReconnecting
                  ? 'Restoring display stream after pause/wake'
                  : 'Establishing low-latency TCP connection',
              style: const TextStyle(color: Colors.white54, fontSize: 13),
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
          // Dropdown menu toggle button ("V" on the left) to collapse banner
          IconButton(
            icon: const Icon(
              Icons.keyboard_arrow_up,
              color: Color(0xFF38BDF8),
              size: 26,
            ),
            tooltip: 'Hide controls',
            padding: EdgeInsets.zero,
            constraints: const BoxConstraints(minWidth: 36, minHeight: 36),
            onPressed: _toggleOverlay,
          ),
          const SizedBox(width: 6),

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

          // Touch Control Toggle
          IconButton(
            icon: Icon(
              _touchInputEnabled ? Icons.touch_app : Icons.touch_app_outlined,
              color: _touchInputEnabled ? Colors.greenAccent : Colors.white38,
              size: 20,
            ),
            tooltip: _touchInputEnabled
                ? 'Disable Touch Input'
                : 'Enable Touch Input',
            onPressed: () {
              setState(() {
                _touchInputEnabled = !_touchInputEnabled;
              });
            },
          ),

          // Input Mode Toggle (Direct Touch vs Trackpad)
          IconButton(
            icon: Icon(
              _inputMode == TouchInputMode.direct
                  ? Icons.fingerprint
                  : Icons.mouse,
              color: Colors.white70,
              size: 20,
            ),
            tooltip: _inputMode == TouchInputMode.direct
                ? 'Mode: Direct Touch (Tap to switch to Trackpad)'
                : 'Mode: Trackpad (Tap to switch to Direct Touch)',
            onPressed: () {
              setState(() {
                _inputMode = _inputMode == TouchInputMode.direct
                    ? TouchInputMode.trackpad
                    : TouchInputMode.direct;
              });
            },
          ),

          // On-Screen Keyboard Toggle
          IconButton(
            icon: Icon(
              _showKeyboard ? Icons.keyboard_hide : Icons.keyboard,
              color: _showKeyboard ? Colors.blueAccent : Colors.white70,
              size: 20,
            ),
            tooltip: _showKeyboard ? 'Hide Keyboard' : 'Show Keyboard',
            onPressed: () {
              setState(() {
                _showKeyboard = !_showKeyboard;
              });
            },
          ),

          const SizedBox(width: 8),

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
    final proto = state.activeProtocol == StreamingProtocol.ss02H264
        ? 'H.264 SS02'
        : (state.activeProtocol == StreamingProtocol.ss01Jpeg
              ? 'JPEG SS01'
              : '');
    final protoStr = proto.isNotEmpty ? '[$proto] ' : '';

    if (state.isStreaming) {
      final frameKb = (state.lastFrameSizeBytes / 1024).toStringAsFixed(1);
      final transitStr = state.pingMs != null ? ' • ${state.pingMs} ms' : '';
      return '$protoStr${state.receivedFps} FPS • $frameKb KB/frame$transitStr';
    }
    return '$protoStr${settings.resolution.label} • ${settings.frameRate.fps} FPS'
        '${state.pingMs != null ? " • ${state.pingMs} ms" : ""}';
  }
}
