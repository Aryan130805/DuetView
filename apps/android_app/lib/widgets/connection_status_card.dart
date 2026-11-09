import 'package:flutter/material.dart';

import '../models/connection_state.dart';
import '../theme/app_theme.dart';

/// Redesigned Glassmorphic Connection Status Card matching Reference Image 1
class ConnectionStatusCard extends StatelessWidget {
  final ConnectionStateModel state;
  final VoidCallback? onCancel;
  final VoidCallback? onDisconnect;
  final VoidCallback? onOpenStream;
  final VoidCallback? onTap;

  const ConnectionStatusCard({
    super.key,
    required this.state,
    this.onCancel,
    this.onDisconnect,
    this.onOpenStream,
    this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final isOnline = state.isConnected || state.isStreaming;

    return Container(
      decoration: BoxDecoration(
        color: AppTheme.primaryCard,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: AppTheme.border),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.3),
            blurRadius: 16,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          borderRadius: BorderRadius.circular(20),
          onTap: onTap ?? (isOnline ? onOpenStream : null),
          child: Padding(
            padding: const EdgeInsets.all(18),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    // Monitor Icon Container
                    Container(
                      width: 50,
                      height: 50,
                      decoration: BoxDecoration(
                        color: AppTheme.elevatedCard,
                        borderRadius: BorderRadius.circular(14),
                        border: Border.all(color: AppTheme.border),
                      ),
                      child: Icon(
                        isOnline ? Icons.tv_rounded : Icons.desktop_windows_outlined,
                        color: isOnline ? AppTheme.cyanAccent : AppTheme.secondaryText,
                        size: 24,
                      ),
                    ),
                    const SizedBox(width: 14),

                    // Title & Description
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            AppTheme.labelForStatus(state.status),
                            style: TextStyle(
                              fontSize: 16,
                              fontWeight: FontWeight.bold,
                              color: isOnline ? AppTheme.cyanAccent : AppTheme.primaryText,
                            ),
                          ),
                          const SizedBox(height: 3),
                          Text(
                            _buildSubtitle(),
                            style: const TextStyle(
                              fontSize: 13,
                              color: AppTheme.mutedText,
                            ),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                        ],
                      ),
                    ),

                    // Chevron Arrow
                    const Icon(
                      Icons.chevron_right_rounded,
                      color: AppTheme.mutedText,
                      size: 24,
                    ),
                  ],
                ),

                // Error Message if failed
                if (state.status == ConnectionStatus.failed && state.errorMessage != null) ...[
                  const SizedBox(height: 12),
                  Container(
                    width: double.infinity,
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: AppTheme.error.withValues(alpha: 0.12),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(color: AppTheme.error.withValues(alpha: 0.3)),
                    ),
                    child: Text(
                      state.errorMessage!,
                      style: const TextStyle(
                        color: AppTheme.error,
                        fontSize: 12,
                        fontWeight: FontWeight.w500,
                      ),
                    ),
                  ),
                ],

                // Connected/Streaming Details & Quick Actions
                if (isOnline) ...[
                  const SizedBox(height: 14),
                  const Divider(color: AppTheme.border, height: 1),
                  const SizedBox(height: 12),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Expanded(
                        child: _buildMetaItem('Target Host', state.connectedDevice?.address ?? '127.0.0.1'),
                      ),
                      Expanded(
                        child: _buildMetaItem('Port', '${state.connectedDevice?.port ?? 52025}'),
                      ),
                      Expanded(
                        child: _buildMetaItem(
                          state.isStreaming ? 'Live FPS' : 'Ping',
                          state.isStreaming ? '${state.receivedFps} FPS' : (state.pingMs != null ? '${state.pingMs} ms' : '--'),
                          highlight: state.isStreaming,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      if (onOpenStream != null)
                        Expanded(
                          child: Container(
                            height: 40,
                            decoration: BoxDecoration(
                              gradient: const LinearGradient(
                                colors: [AppTheme.primaryBlue, AppTheme.cyanAccent],
                                begin: Alignment.topLeft,
                                end: Alignment.bottomRight,
                              ),
                              borderRadius: BorderRadius.circular(10),
                            ),
                            child: Material(
                              color: Colors.transparent,
                              child: InkWell(
                                borderRadius: BorderRadius.circular(10),
                                onTap: onOpenStream,
                                child: Center(
                                  child: Row(
                                    mainAxisAlignment: MainAxisAlignment.center,
                                    children: [
                                      const Icon(Icons.fullscreen, color: Colors.white, size: 18),
                                      const SizedBox(width: 6),
                                      Text(
                                        state.isStreaming
                                            ? 'View Live Stream'
                                            : 'Open Display',
                                        style: TextStyle(
                                          color: Colors.white,
                                          fontWeight: FontWeight.bold,
                                          fontSize: 13,
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ),
                            ),
                          ),
                        ),
                      if (onDisconnect != null) ...[
                        const SizedBox(width: 10),
                        OutlinedButton.icon(
                          onPressed: onDisconnect,
                          icon: const Icon(Icons.link_off, size: 16),
                          label: const Text('Disconnect'),
                          style: OutlinedButton.styleFrom(
                            foregroundColor: AppTheme.error,
                            side: const BorderSide(color: AppTheme.error),
                            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
                            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                          ),
                        ),
                      ],
                    ],
                  ),
                ],

                // Connecting: Cancel button
                if (state.status == ConnectionStatus.connecting && onCancel != null) ...[
                  const SizedBox(height: 12),
                  OutlinedButton(
                    onPressed: onCancel,
                    style: OutlinedButton.styleFrom(
                      foregroundColor: AppTheme.secondaryText,
                      side: const BorderSide(color: AppTheme.border),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                    ),
                    child: const Text('Cancel Connection'),
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildMetaItem(String label, String value, {bool highlight = false}) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          label,
          style: const TextStyle(
            fontSize: 11,
            color: AppTheme.mutedText,
            fontWeight: FontWeight.w500,
          ),
        ),
        const SizedBox(height: 2),
        Text(
          value,
          style: TextStyle(
            fontSize: 13,
            fontWeight: FontWeight.bold,
            color: highlight ? AppTheme.success : AppTheme.primaryText,
          ),
          overflow: TextOverflow.ellipsis,
          maxLines: 1,
        ),
      ],
    );
  }

  String _buildSubtitle() {
    switch (state.status) {
      case ConnectionStatus.disconnected:
        return 'No active connection to a Windows PC.';
      case ConnectionStatus.searching:
        return 'Broadcasting UDP discovery requests...';
      case ConnectionStatus.connecting:
        return 'Connecting to ${state.connectedDevice?.name ?? 'host'}...';
      case ConnectionStatus.connected:
        return 'Established TCP session. Initializing decoder...';
      case ConnectionStatus.streaming:
        return 'Hardware-accelerated H.264 display stream active.';
      case ConnectionStatus.failed:
        return state.errorMessage ?? 'Connection dropped or timed out.';
    }
  }
}
