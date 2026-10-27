import 'package:flutter/material.dart';

import '../models/connection_state.dart';
import '../theme/app_theme.dart';

class ConnectionStatusCard extends StatelessWidget {
  final ConnectionStateModel state;
  final VoidCallback? onCancel;
  final VoidCallback? onDisconnect;
  final VoidCallback? onOpenStream;

  const ConnectionStatusCard({
    super.key,
    required this.state,
    this.onCancel,
    this.onDisconnect,
    this.onOpenStream,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final statusColor = AppTheme.colorForStatus(state.status);

    return Card(
      clipBehavior: Clip.antiAlias,
      child: Container(
        decoration: BoxDecoration(
          border: Border(left: BorderSide(color: statusColor, width: 4)),
        ),
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                _buildStatusIcon(state.status, statusColor),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        AppTheme.labelForStatus(state.status),
                        style: theme.textTheme.titleMedium?.copyWith(
                          fontWeight: FontWeight.w700,
                          color: statusColor,
                        ),
                      ),
                      const SizedBox(height: 2),
                      Text(
                        _buildSubtitle(),
                        style: theme.textTheme.bodyMedium?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            if (state.status == ConnectionStatus.failed &&
                state.errorMessage != null) ...[
              const SizedBox(height: 12),
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: AppTheme.statusFailed.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(
                    color: AppTheme.statusFailed.withValues(alpha: 0.3),
                  ),
                ),
                child: Text(
                  state.errorMessage!,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: AppTheme.statusFailed,
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ),
            ],
            if (state.status == ConnectionStatus.connected ||
                state.status == ConnectionStatus.streaming) ...[
              const SizedBox(height: 16),
              const Divider(height: 1),
              const SizedBox(height: 12),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  _buildMetaItem(
                    theme,
                    label: 'Target Host',
                    value: state.connectedDevice?.address ?? 'Local IP',
                  ),
                  _buildMetaItem(
                    theme,
                    label: 'Port',
                    value: '${state.connectedDevice?.port ?? 52025}',
                  ),
                  _buildMetaItem(
                    theme,
                    label: state.isStreaming ? 'Live FPS' : 'Ping',
                    value: state.isStreaming
                        ? '${state.receivedFps} FPS'
                        : (state.pingMs != null ? '${state.pingMs} ms' : '--'),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  if (onOpenStream != null)
                    Expanded(
                      child: FilledButton.icon(
                        onPressed: onOpenStream,
                        icon: const Icon(Icons.fullscreen, size: 18),
                        label: Text(
                          state.isStreaming
                              ? 'View Live Stream'
                              : 'Open Display',
                        ),
                      ),
                    ),
                  if (onDisconnect != null) ...[
                    const SizedBox(width: 12),
                    OutlinedButton.icon(
                      onPressed: onDisconnect,
                      icon: const Icon(Icons.link_off, size: 18),
                      label: const Text('Disconnect'),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: theme.colorScheme.error,
                        side: BorderSide(color: theme.colorScheme.error),
                      ),
                    ),
                  ],
                ],
              ),
            ],
            if (state.status == ConnectionStatus.connecting &&
                onCancel != null) ...[
              const SizedBox(height: 16),
              SizedBox(
                width: double.infinity,
                child: OutlinedButton(
                  onPressed: onCancel,
                  child: const Text('Cancel Connection'),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildStatusIcon(ConnectionStatus status, Color color) {
    switch (status) {
      case ConnectionStatus.disconnected:
        return Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.12),
            shape: BoxShape.circle,
          ),
          child: Icon(Icons.cast, color: color, size: 24),
        );
      case ConnectionStatus.searching:
        return Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.12),
            shape: BoxShape.circle,
          ),
          child: SizedBox(
            width: 24,
            height: 24,
            child: CircularProgressIndicator(
              strokeWidth: 2.5,
              valueColor: AlwaysStoppedAnimation<Color>(color),
            ),
          ),
        );
      case ConnectionStatus.connecting:
        return Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.12),
            shape: BoxShape.circle,
          ),
          child: SizedBox(
            width: 24,
            height: 24,
            child: CircularProgressIndicator(
              strokeWidth: 2.5,
              valueColor: AlwaysStoppedAnimation<Color>(color),
            ),
          ),
        );
      case ConnectionStatus.connected:
        return Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.12),
            shape: BoxShape.circle,
          ),
          child: Icon(Icons.sync, color: color, size: 24),
        );
      case ConnectionStatus.streaming:
        return Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.12),
            shape: BoxShape.circle,
          ),
          child: Icon(Icons.live_tv, color: color, size: 24),
        );
      case ConnectionStatus.failed:
        return Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.12),
            shape: BoxShape.circle,
          ),
          child: Icon(Icons.error_outline, color: color, size: 24),
        );
    }
  }

  String _buildSubtitle() {
    switch (state.status) {
      case ConnectionStatus.disconnected:
        return 'Not connected to any Windows PC.';
      case ConnectionStatus.searching:
        return 'Listening for broadcast beacons on your local network.';
      case ConnectionStatus.connecting:
        final dev = state.connectedDevice;
        return 'Handshaking with ${dev?.address ?? "host"}...';
      case ConnectionStatus.connected:
        final dev = state.connectedDevice;
        return 'Connected to ${dev?.name ?? "Windows PC"}. Waiting for frames...';
      case ConnectionStatus.streaming:
        final dev = state.connectedDevice;
        return 'Streaming desktop from ${dev?.address} at ${state.receivedFps} FPS.';
      case ConnectionStatus.failed:
        return 'Could not establish connection to the host.';
    }
  }

  Widget _buildMetaItem(
    ThemeData theme, {
    required String label,
    required String value,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          label.toUpperCase(),
          style: theme.textTheme.labelSmall?.copyWith(
            letterSpacing: 0.8,
            color: theme.colorScheme.onSurfaceVariant,
            fontWeight: FontWeight.w600,
          ),
        ),
        const SizedBox(height: 2),
        Text(
          value,
          style: theme.textTheme.bodyMedium?.copyWith(
            fontWeight: FontWeight.w700,
          ),
        ),
      ],
    );
  }
}
