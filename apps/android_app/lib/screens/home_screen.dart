import 'package:flutter/material.dart';

import '../services/connection_service.dart';
import '../services/settings_service.dart';
import '../widgets/connection_status_card.dart';
import '../widgets/primary_button.dart';
import '../widgets/responsive_container.dart';
import 'connection_screen.dart';
import 'settings_screen.dart';
import 'streaming_screen.dart';

class HomeScreen extends StatelessWidget {
  final ConnectionService connectionService;
  final SettingsService settingsService;

  const HomeScreen({
    super.key,
    required this.connectionService,
    required this.settingsService,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Row(
          children: [
            Icon(Icons.desktop_windows, size: 22),
            SizedBox(width: 8),
            Text('SecondScreen'),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings_outlined),
            tooltip: 'Settings',
            onPressed: () {
              Navigator.push(
                context,
                MaterialPageRoute(
                  builder: (context) =>
                      SettingsScreen(settingsService: settingsService),
                ),
              );
            },
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          child: ResponsiveContainer(
            child: ListenableBuilder(
              listenable: connectionService,
              builder: (context, _) {
                final state = connectionService.state;

                return Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const SizedBox(height: 8),
                    // Hero description
                    Text(
                      'Turn your Android device into a second display for your PC.',
                      style: theme.textTheme.headlineSmall?.copyWith(
                        fontWeight: FontWeight.w700,
                        letterSpacing: -0.5,
                      ),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'High-performance desktop extension over local Wi-Fi or USB. Completely free, local, and private.',
                      style: theme.textTheme.bodyMedium?.copyWith(
                        color: theme.colorScheme.onSurfaceVariant,
                      ),
                    ),
                    const SizedBox(height: 24),

                    // Connection status card
                    ConnectionStatusCard(
                      state: state,
                      onCancel: () => connectionService.cancelConnection(),
                      onDisconnect: () => connectionService.disconnect(),
                      onOpenStream: () {
                        Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => StreamingScreen(
                              connectionService: connectionService,
                              settingsService: settingsService,
                            ),
                          ),
                        );
                      },
                    ),
                    const SizedBox(height: 20),

                    // Actions
                    if (state.isConnected || state.isStreaming) ...[
                      PrimaryButton(
                        label: state.isStreaming
                            ? 'View Live Stream'
                            : 'View Extended Display',
                        icon: Icons.fullscreen,
                        onPressed: () {
                          Navigator.push(
                            context,
                            MaterialPageRoute(
                              builder: (context) => StreamingScreen(
                                connectionService: connectionService,
                                settingsService: settingsService,
                              ),
                            ),
                          );
                        },
                      ),
                    ] else ...[
                      PrimaryButton(
                        label: 'Connect to PC',
                        icon: Icons.link,
                        isLoading: state.isConnecting,
                        onPressed: () => _openConnectScreen(context),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: () => _openConnectScreen(
                                context,
                                autoStartDiscovery: true,
                              ),
                              icon: const Icon(Icons.radar, size: 18),
                              label: const Text('Discover PCs'),
                              style: OutlinedButton.styleFrom(
                                padding: const EdgeInsets.symmetric(
                                  vertical: 14,
                                ),
                                shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(12),
                                ),
                              ),
                            ),
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: () =>
                                  _openConnectScreen(context, manualOnly: true),
                              icon: const Icon(Icons.edit_note, size: 18),
                              label: const Text('Manual IP'),
                              style: OutlinedButton.styleFrom(
                                padding: const EdgeInsets.symmetric(
                                  vertical: 14,
                                ),
                                shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(12),
                                ),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ],

                    const SizedBox(height: 32),

                    // Local Network Advisory
                    _buildNetworkAdvisory(theme),
                    const SizedBox(height: 16),
                  ],
                );
              },
            ),
          ),
        ),
      ),
    );
  }

  void _openConnectScreen(
    BuildContext context, {
    bool autoStartDiscovery = false,
    bool manualOnly = false,
  }) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => ConnectionScreen(
          connectionService: connectionService,
          settingsService: settingsService,
          autoStartDiscovery: autoStartDiscovery,
          initialTab: manualOnly ? 1 : 0,
        ),
      ),
    );
  }

  Widget _buildNetworkAdvisory(ThemeData theme) {
    return Card(
      color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.4),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  Icons.wifi_tethering,
                  size: 20,
                  color: theme.colorScheme.primary,
                ),
                const SizedBox(width: 8),
                Text(
                  'Local Network Requirement',
                  style: theme.textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              'Both your Windows PC and this Android device must be connected to the same local network router (5 GHz Wi-Fi recommended for 60 FPS). '
              'Ensure Windows Firewall allows inbound connections on the configured streaming port (default: 52025).',
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
                height: 1.4,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
