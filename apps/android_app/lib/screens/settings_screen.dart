import 'package:flutter/material.dart';

import '../models/stream_settings.dart';
import '../services/settings_service.dart';
import '../services/url_launcher_service.dart';
import '../widgets/download_pc_guide_sheet.dart';
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
                    const SizedBox(height: 12),
                    // Section 1: Display Preferences
                    _buildSectionHeader(theme, 'Display Preferences'),
                    const SizedBox(height: 8),
                    Card(
                      child: Column(
                        children: [
                          // Preferred Resolution
                          _buildDropdownRow<StreamResolution>(
                            icon: Icons.aspect_ratio,
                            label: 'Preferred Resolution',
                            value: settings.resolution,
                            items: StreamResolution.values.map((res) {
                              return DropdownMenuItem(
                                value: res,
                                child: Text(
                                  res.label,
                                  style: const TextStyle(
                                    fontSize: 13,
                                    fontWeight: FontWeight.w600,
                                    color: Colors.white,
                                  ),
                                ),
                              );
                            }).toList(),
                            onChanged: (newRes) {
                              if (newRes != null) {
                                settingsService.updateSettings(
                                  settings.copyWith(resolution: newRes),
                                );
                              }
                            },
                            theme: theme,
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),

                          // Preferred Frame Rate
                          _buildDropdownRow<StreamFrameRate>(
                            icon: Icons.speed,
                            label: 'Preferred Frame Rate',
                            value: settings.frameRate,
                            items: StreamFrameRate.values.map((fps) {
                              return DropdownMenuItem(
                                value: fps,
                                child: Text(
                                  fps.label,
                                  style: const TextStyle(
                                    fontSize: 13,
                                    fontWeight: FontWeight.w600,
                                    color: Colors.white,
                                  ),
                                ),
                              );
                            }).toList(),
                            onChanged: (newFps) {
                              if (newFps != null) {
                                settingsService.updateSettings(
                                  settings.copyWith(frameRate: newFps),
                                );
                              }
                            },
                            theme: theme,
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),

                          // Full-Screen Mode
                          _buildSwitchRow(
                            icon: Icons.fullscreen,
                            title: 'Full-Screen Mode',
                            subtitle:
                                'Hide Android status and navigation bars during streaming',
                            value: settings.fullScreen,
                            onChanged: (val) {
                              settingsService.updateSettings(
                                settings.copyWith(fullScreen: val),
                              );
                            },
                            theme: theme,
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
                          Padding(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 16,
                              vertical: 12,
                            ),
                            child: Row(
                              children: [
                                const Icon(
                                  Icons.wifi,
                                  color: Color(0xFF38BDF8),
                                  size: 22,
                                ),
                                const SizedBox(width: 14),
                                Expanded(
                                  child: Column(
                                    crossAxisAlignment:
                                        CrossAxisAlignment.start,
                                    children: [
                                      const Text(
                                        'Transport Protocol',
                                        style: TextStyle(
                                          fontSize: 14,
                                          fontWeight: FontWeight.w600,
                                          color: Colors.white,
                                        ),
                                      ),
                                      const SizedBox(height: 2),
                                      Text(
                                        settings.transport.label,
                                        style: const TextStyle(
                                          fontSize: 12,
                                          color: Color(0xFF94A3B8),
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                                Container(
                                  padding: const EdgeInsets.symmetric(
                                    horizontal: 8,
                                    vertical: 4,
                                  ),
                                  decoration: BoxDecoration(
                                    color: const Color(0xFF10B981)
                                        .withValues(alpha: 0.15),
                                    borderRadius: BorderRadius.circular(6),
                                    border: Border.all(
                                      color: const Color(0xFF10B981)
                                          .withValues(alpha: 0.4),
                                    ),
                                  ),
                                  child: const Text(
                                    'Wi-Fi active',
                                    style: TextStyle(
                                      fontSize: 11,
                                      fontWeight: FontWeight.bold,
                                      color: Color(0xFF10B981),
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),
                          _buildSwitchRow(
                            icon: Icons.autorenew,
                            title: 'Automatic Reconnection',
                            subtitle:
                                'Attempt to reconnect if the connection drops unexpectedly',
                            value: settings.autoReconnect,
                            onChanged: (val) {
                              settingsService.updateSettings(
                                settings.copyWith(autoReconnect: val),
                              );
                            },
                            theme: theme,
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
                                'Note: Resolution and frame rate options (supporting up to 120 FPS) are saved as client preferences and negotiate target streaming parameters with the host PC. '
                                'Actual rendered FPS depends on physical display hardware, GPU encoding capability, and local Wi-Fi bandwidth.',
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
                    _buildSectionHeader(theme, 'About DuetView'),
                    const SizedBox(height: 8),
                    Card(
                      child: Column(
                        children: [
                          ListTile(
                            leading: const Icon(Icons.person, color: Color(0xFF38BDF8)),
                            title: const Text('Developer'),
                            subtitle: const Text('Aryan Verma (@Aryan130805)\nTap to view GitHub profile'),
                            isThreeLine: true,
                            trailing: const Icon(Icons.open_in_new, size: 18, color: Color(0xFF38BDF8)),
                            onTap: () => UrlLauncherService.openUrl(
                              context,
                              UrlLauncherService.developerProfile,
                              title: 'Developer GitHub Profile',
                            ),
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),
                          ListTile(
                            leading: const Icon(Icons.laptop_windows_rounded, color: Color(0xFF38BDF8)),
                            title: const Text('DuetView PC (Windows)'),
                            subtitle: const Text('Download companion application from GitHub Releases'),
                            trailing: const Icon(Icons.open_in_new, size: 18, color: Color(0xFF38BDF8)),
                            onTap: () => DownloadPcGuideSheet.show(context),
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),
                          ListTile(
                            leading: const Icon(Icons.code, color: Color(0xFF38BDF8)),
                            title: const Text('Open Source Project'),
                            subtitle: const Text(
                              'Explore source code, releases, and documentation on GitHub',
                            ),
                            trailing: const Icon(Icons.open_in_new, size: 18, color: Color(0xFF38BDF8)),
                            onTap: () => UrlLauncherService.openUrl(
                              context,
                              UrlLauncherService.projectRepository,
                              title: 'DuetView GitHub Repository',
                            ),
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),
                          const ListTile(
                            leading: Icon(Icons.devices, color: Color(0xFF38BDF8)),
                            title: Text('Project Scope'),
                            subtitle: Text(
                              'DuetView — Secondary display system for Windows and Android providing low-latency desktop extension and mirroring over local Wi-Fi or USB.',
                            ),
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),
                          const ListTile(
                            leading: Icon(Icons.info, color: Color(0xFF38BDF8)),
                            title: Text('Application Version'),
                            subtitle: Text('1.0.0 (MVP Build)'),
                          ),
                          const Divider(height: 1, indent: 52, endIndent: 16),
                          const ListTile(
                            leading: Icon(
                              Icons.lock_outline,
                              color: Color(0xFF38BDF8),
                            ),
                            title: Text('Privacy & Security'),
                            subtitle: Text(
                              'Zero cloud servers. Screen transmission and control remain 100% peer-to-peer on your local network.',
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

  Widget _buildDropdownRow<T>({
    required IconData icon,
    required String label,
    required T value,
    required List<DropdownMenuItem<T>> items,
    required ValueChanged<T?> onChanged,
    required ThemeData theme,
  }) {
    return LayoutBuilder(
      builder: (context, constraints) {
        // In narrow layouts (width < 360), stack label and selector so text never breaks awkwardly
        final bool isCompact = constraints.maxWidth < 360;

        final dropdownWidget = Container(
          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
          decoration: BoxDecoration(
            color: const Color(0xFF0F172A).withValues(alpha: 0.6),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: const Color(0xFF334155)),
          ),
          child: DropdownButtonHideUnderline(
            child: DropdownButton<T>(
              value: value,
              isExpanded: true,
              isDense: true,
              dropdownColor: const Color(0xFF1E293B),
              borderRadius: BorderRadius.circular(10),
              icon: const Icon(Icons.arrow_drop_down, color: Color(0xFF38BDF8)),
              items: items,
              onChanged: onChanged,
            ),
          ),
        );

        if (isCompact) {
          return Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Icon(icon, color: const Color(0xFF38BDF8), size: 22),
                    const SizedBox(width: 14),
                    Expanded(
                      child: Text(
                        label,
                        style: const TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w600,
                          color: Colors.white,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                dropdownWidget,
              ],
            ),
          );
        }

        return Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
          child: Row(
            children: [
              Icon(icon, color: const Color(0xFF38BDF8), size: 22),
              const SizedBox(width: 14),
              Expanded(
                child: Text(
                  label,
                  style: const TextStyle(
                    fontSize: 14,
                    fontWeight: FontWeight.w600,
                    color: Colors.white,
                  ),
                ),
              ),
              const SizedBox(width: 12),
              ConstrainedBox(
                constraints: const BoxConstraints(minWidth: 150, maxWidth: 210),
                child: dropdownWidget,
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildSwitchRow({
    required IconData icon,
    required String title,
    required String subtitle,
    required bool value,
    required ValueChanged<bool> onChanged,
    required ThemeData theme,
  }) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          Icon(icon, color: const Color(0xFF38BDF8), size: 22),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 14,
                    fontWeight: FontWeight.w600,
                    color: Colors.white,
                  ),
                ),
                const SizedBox(height: 3),
                Text(
                  subtitle,
                  style: const TextStyle(
                    fontSize: 12,
                    color: Color(0xFF94A3B8),
                    height: 1.35,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          Switch(
            value: value,
            onChanged: onChanged,
            activeThumbColor: const Color(0xFF38BDF8),
            activeTrackColor: const Color(0xFF0284C7).withValues(alpha: 0.5),
            inactiveThumbColor: const Color(0xFF94A3B8),
            inactiveTrackColor: const Color(0xFF334155),
          ),
        ],
      ),
    );
  }
}
