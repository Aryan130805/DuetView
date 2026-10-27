import 'package:flutter/material.dart';

import '../models/stream_settings.dart';
import '../services/settings_service.dart';
import '../widgets/responsive_container.dart';

class SettingsScreen extends StatelessWidget {
  final SettingsService settingsService;

  const SettingsScreen({super.key, required this.settingsService});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: SafeArea(
        child: SingleChildScrollView(
          child: ResponsiveContainer(
            child: ListenableBuilder(
              listenable: settingsService,
              builder: (context, _) {
                final settings = settingsService.settings;

                return Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    // Section 1: Display Preferences
                    _buildSectionHeader(theme, 'Display Preferences'),
                    const SizedBox(height: 8),
                    Card(
                      child: Column(
                        children: [
                          ListTile(
                            leading: const Icon(Icons.aspect_ratio),
                            title: const Text('Preferred Resolution'),
                            subtitle: Text(settings.resolution.label),
                            trailing: DropdownButton<StreamResolution>(
                              value: settings.resolution,
                              underline: const SizedBox(),
                              items: StreamResolution.values.map((res) {
                                return DropdownMenuItem(
                                  value: res,
                                  child: Text(res.label),
                                );
                              }).toList(),
                              onChanged: (newRes) {
                                if (newRes != null) {
                                  settingsService.updateSettings(
                                    settings.copyWith(resolution: newRes),
                                  );
                                }
                              },
                            ),
                          ),
                          const Divider(height: 1, indent: 16, endIndent: 16),
                          ListTile(
                            leading: const Icon(Icons.speed),
                            title: const Text('Preferred Frame Rate'),
                            subtitle: Text(settings.frameRate.label),
                            trailing: DropdownButton<StreamFrameRate>(
                              value: settings.frameRate,
                              underline: const SizedBox(),
                              items: StreamFrameRate.values.map((fps) {
                                return DropdownMenuItem(
                                  value: fps,
                                  child: Text(fps.label),
                                );
                              }).toList(),
                              onChanged: (newFps) {
                                if (newFps != null) {
                                  settingsService.updateSettings(
                                    settings.copyWith(frameRate: newFps),
                                  );
                                }
                              },
                            ),
                          ),
                          const Divider(height: 1, indent: 16, endIndent: 16),
                          SwitchListTile(
                            secondary: const Icon(Icons.fullscreen),
                            title: const Text('Full-Screen Mode'),
                            subtitle: const Text(
                              'Hide Android status and navigation bars during streaming',
                            ),
                            value: settings.fullScreen,
                            onChanged: (val) {
                              settingsService.updateSettings(
                                settings.copyWith(fullScreen: val),
                              );
                            },
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 24),

                    // Section 2: Connection Preferences
                    _buildSectionHeader(theme, 'Connection Preferences'),
                    const SizedBox(height: 8),
                    Card(
                      child: Column(
                        children: [
                          ListTile(
                            leading: const Icon(Icons.wifi),
                            title: const Text('Transport Protocol'),
                            subtitle: Text(settings.transport.label),
                            trailing: const Chip(
                              label: Text('Wi-Fi active'),
                              visualDensity: VisualDensity.compact,
                            ),
                          ),
                          const Divider(height: 1, indent: 16, endIndent: 16),
                          SwitchListTile(
                            secondary: const Icon(Icons.autorenew),
                            title: const Text('Automatic Reconnection'),
                            subtitle: const Text(
                              'Attempt to reconnect if the connection drops unexpectedly',
                            ),
                            value: settings.autoReconnect,
                            onChanged: (val) {
                              settingsService.updateSettings(
                                settings.copyWith(autoReconnect: val),
                              );
                            },
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 24),

                    // Section 3: Technical Note
                    Card(
                      color: theme.colorScheme.surfaceContainerHighest
                          .withValues(alpha: 0.35),
                      child: Padding(
                        padding: const EdgeInsets.all(16),
                        child: Row(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Icon(
                              Icons.info_outline,
                              size: 20,
                              color: theme.colorScheme.primary,
                            ),
                            const SizedBox(width: 12),
                            Expanded(
                              child: Text(
                                'Note: Resolution and frame rate options are saved as client preferences. '
                                'In Phase 4 and 5, these preferences will be dynamically negotiated with the Windows Indirect Display Driver (IddCx).',
                                style: theme.textTheme.bodySmall?.copyWith(
                                  color: theme.colorScheme.onSurfaceVariant,
                                  height: 1.4,
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                    const SizedBox(height: 24),

                    // Section 4: About & Version
                    _buildSectionHeader(theme, 'About SecondScreen'),
                    const SizedBox(height: 8),
                    Card(
                      child: Column(
                        children: [
                          const ListTile(
                            leading: Icon(Icons.info),
                            title: Text('Application Version'),
                            subtitle: Text('1.0.0 (MVP Build)'),
                          ),
                          const Divider(height: 1, indent: 16, endIndent: 16),
                          const ListTile(
                            leading: Icon(Icons.lock_outline),
                            title: Text('Privacy & Security'),
                            subtitle: Text(
                              'Zero cloud servers. Screen transmission and control remain 100% peer-to-peer on your local network.',
                            ),
                          ),
                          const Divider(height: 1, indent: 16, endIndent: 16),
                          const ListTile(
                            leading: Icon(Icons.code),
                            title: Text('Open Source Project'),
                            subtitle: Text(
                              'Free, non-commercial software built with Flutter & modern Windows systems APIs.',
                            ),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 24),
                  ],
                );
              },
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildSectionHeader(ThemeData theme, String title) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 4),
      child: Text(
        title,
        style: theme.textTheme.labelLarge?.copyWith(
          fontWeight: FontWeight.w700,
          color: theme.colorScheme.primary,
          letterSpacing: 0.5,
        ),
      ),
    );
  }
}
