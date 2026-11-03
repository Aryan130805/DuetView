import 'package:flutter/material.dart';

import '../services/url_launcher_service.dart';
import '../widgets/download_pc_guide_sheet.dart';
import '../widgets/responsive_container.dart';

/// In-app user guide explaining DuetView wireless and wired connection workflows,
/// display modes, touch gestures, and troubleshooting steps.
class GuideScreen extends StatelessWidget {
  const GuideScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('How to Use DuetView'),
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          child: ResponsiveContainer(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const SizedBox(height: 12),
                // Hero Header Card
                Container(
                  padding: const EdgeInsets.all(20),
                  decoration: BoxDecoration(
                    gradient: const LinearGradient(
                      colors: [Color(0xFF0F172A), Color(0xFF1E293B)],
                      begin: Alignment.topLeft,
                      end: Alignment.bottomRight,
                    ),
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(color: const Color(0xFF334155)),
                  ),
                  child: Row(
                    children: [
                      Container(
                        width: 56,
                        height: 56,
                        decoration: BoxDecoration(
                          borderRadius: BorderRadius.circular(12),
                          image: const DecorationImage(
                            image: AssetImage('assets/images/duetview_icon.png'),
                            fit: BoxFit.cover,
                          ),
                        ),
                      ),
                      const SizedBox(width: 16),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              'Welcome to DuetView',
                              style: theme.textTheme.titleMedium?.copyWith(
                                fontWeight: FontWeight.bold,
                                color: Colors.white,
                              ),
                            ),
                            const SizedBox(height: 4),
                            Text(
                              'Transform your Android device into a responsive, high-performance display for your Windows PC.',
                              style: theme.textTheme.bodySmall?.copyWith(
                                color: const Color(0xFF94A3B8),
                                height: 1.4,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 20),

                // DuetView PC Companion Banner
                _buildPcDownloadBanner(context, theme),
                const SizedBox(height: 24),

                // Quick Navigation Pills / Summary
                _buildSectionHeader(
                  title: 'Connection Methods',
                  subtitle: 'Choose how to connect your Android device to your PC',
                ),
                const SizedBox(height: 12),

                // SECTION A: Connect Wirelessly
                _buildWirelessSection(theme),
                const SizedBox(height: 20),

                // SECTION B: Connect with USB
                _buildUsbSection(theme),
                const SizedBox(height: 24),

                // SECTION C: Display Modes
                _buildSectionHeader(
                  title: 'Display Modes',
                  subtitle: 'Mirroring vs. True Extended Desktop',
                ),
                const SizedBox(height: 12),
                _buildDisplayModesSection(theme),
                const SizedBox(height: 24),

                // SECTION D: Touch and Keyboard Controls
                _buildSectionHeader(
                  title: 'Controls & Gestures',
                  subtitle: 'How to interact with your Windows desktop from touch',
                ),
                const SizedBox(height: 12),
                _buildControlsSection(theme),
                const SizedBox(height: 24),

                // SECTION E: Troubleshooting & Diagnostics
                _buildSectionHeader(
                  title: 'Troubleshooting Guide',
                  subtitle: 'Solutions for common network and display issues',
                ),
                const SizedBox(height: 12),
                _buildTroubleshootingSection(theme),
                const SizedBox(height: 32),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildSectionHeader({
    required String title,
    required String subtitle,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          title,
          style: const TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
            color: Colors.white,
            letterSpacing: -0.3,
          ),
        ),
        const SizedBox(height: 4),
        Text(
          subtitle,
          style: const TextStyle(
            fontSize: 13,
            color: Color(0xFF94A3B8),
          ),
        ),
      ],
    );
  }

  Widget _buildWirelessSection(ThemeData theme) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF1E293B),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF334155)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: const Color(0xFF38BDF8).withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(Icons.wifi, color: Color(0xFF38BDF8), size: 22),
                ),
                const SizedBox(width: 12),
                const Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Connect Wirelessly',
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                          color: Colors.white,
                        ),
                      ),
                      Text(
                        'Recommended for convenient, cable-free secondary display',
                        style: TextStyle(fontSize: 12, color: Color(0xFF94A3B8)),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            const Divider(color: Color(0xFF334155), height: 1),
            const SizedBox(height: 16),
            _buildStepItem(1, 'Launch DuetView PC on your Windows computer.'),
            _buildStepItem(2, 'Confirm the status badge displays "● HOST ONLINE".'),
            _buildStepItem(3, 'For Extended Display mode, install the virtual driver if prompted.'),
            _buildStepItem(4, 'Connect Android and PC to the same Wi-Fi network (5 GHz recommended).'),
            _buildStepItem(5, 'Open DuetView on your Android phone or tablet.'),
            _buildStepItem(6, 'Tap the prominent "Scan to Connect" button on the home screen.'),
            _buildStepItem(7, 'Point your camera at the QR code displayed in DuetView PC.'),
            _buildStepItem(8, 'Wait for automatic authorization and token validation.'),
            _buildStepItem(9, 'The connection status will transition from Connecting to Streaming.'),
            _buildStepItem(10, 'Your Windows desktop now streams in high-definition H.264 video!'),
            _buildStepItem(11, 'Press Win + P on your PC to choose between Mirror and Extend.'),
          ],
        ),
      ),
    );
  }

  Widget _buildUsbSection(ThemeData theme) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF1E293B),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF334155)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: const Color(0xFFF59E0B).withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(Icons.usb, color: Color(0xFFF59E0B), size: 22),
                ),
                const SizedBox(width: 12),
                const Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Connect with USB',
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                          color: Colors.white,
                        ),
                      ),
                      Text(
                        'Ultra-low latency connection via USB cable',
                        style: TextStyle(fontSize: 12, color: Color(0xFF94A3B8)),
                      ),
                    ],
                  ),
                ),
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                  decoration: BoxDecoration(
                    color: const Color(0xFFF59E0B).withValues(alpha: 0.2),
                    borderRadius: BorderRadius.circular(6),
                    border: Border.all(color: const Color(0xFFF59E0B).withValues(alpha: 0.4)),
                  ),
                  child: const Text(
                    'Tethering Mode',
                    style: TextStyle(fontSize: 11, color: Color(0xFFF59E0B), fontWeight: FontWeight.bold),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            const Divider(color: Color(0xFF334155), height: 1),
            const SizedBox(height: 14),

            // Clarification on USB support status
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: const Color(0xFF0F172A),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(color: const Color(0xFF334155)),
              ),
              child: const Row(
                children: [
                  Icon(Icons.info_outline, size: 20, color: Color(0xFF38BDF8)),
                  SizedBox(width: 10),
                  Expanded(
                    child: Text(
                      'Direct raw USB driver-less streaming is in development. Today, wired USB streaming is supported via Android USB Tethering.',
                      style: TextStyle(fontSize: 12, color: Color(0xFFCBD5E1), height: 1.4),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 14),

            _buildStepItem(1, 'Connect your Android device to the PC using a USB data cable (ensure it is not a charge-only cable).'),
            _buildStepItem(2, 'On Android, open Settings > Network & internet > Hotspot & tethering.'),
            _buildStepItem(3, 'Toggle on "USB tethering". Windows will install a high-speed virtual network adapter.'),
            _buildStepItem(4, 'In DuetView PC, click "Start Host Service". Notice the reachable LAN IP (e.g. 192.168.42.x).'),
            _buildStepItem(5, 'On Android, tap "Scan to Connect" or "Manual IP" to connect directly over the USB link with minimal latency!'),
            const SizedBox(height: 8),

            const Text(
              'Note: ADB port forwarding is supported for developers (adb forward tcp:52025 tcp:52025) but is not required for standard tethering.',
              style: TextStyle(fontSize: 11, color: Color(0xFF64748B), fontStyle: FontStyle.italic),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDisplayModesSection(ThemeData theme) {
    return Column(
      children: [
        // Screen Mirroring Card
        Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            color: const Color(0xFF1E293B),
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: const Color(0xFF334155)),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Icon(Icons.copy, color: Color(0xFF38BDF8), size: 20),
                  const SizedBox(width: 10),
                  const Expanded(
                    child: Text(
                      'Screen Mirroring',
                      style: TextStyle(fontWeight: FontWeight.bold, color: Colors.white, fontSize: 15),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                    decoration: BoxDecoration(
                      color: const Color(0xFF10B981).withValues(alpha: 0.15),
                      borderRadius: BorderRadius.circular(6),
                    ),
                    child: const Text(
                      'No Driver Needed',
                      style: TextStyle(color: Color(0xFF10B981), fontSize: 11, fontWeight: FontWeight.bold),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              const Text(
                'Duplicates your primary Windows monitor in real-time on your phone or tablet. Ideal for presentations, viewing from another room, or touch navigation of existing apps.',
                style: TextStyle(color: Color(0xFF94A3B8), fontSize: 13, height: 1.4),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),

        // Extended Display Card
        Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            color: const Color(0xFF1E293B),
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: const Color(0xFF334155)),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Icon(Icons.splitscreen, color: Color(0xFF10B981), size: 20),
                  const SizedBox(width: 10),
                  const Expanded(
                    child: Text(
                      'Extended Display (Second Monitor)',
                      style: TextStyle(fontWeight: FontWeight.bold, color: Colors.white, fontSize: 15),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                    decoration: BoxDecoration(
                      color: const Color(0xFF38BDF8).withValues(alpha: 0.15),
                      borderRadius: BorderRadius.circular(6),
                    ),
                    child: const Text(
                      'Requires Driver',
                      style: TextStyle(color: Color(0xFF38BDF8), fontSize: 11, fontWeight: FontWeight.bold),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              const Text(
                'Creates an independent secondary monitor in Windows. You can drag windows, browsers, and documents onto the Android screen, expanding your desktop workspace.',
                style: TextStyle(color: Color(0xFF94A3B8), fontSize: 13, height: 1.4),
              ),
              const SizedBox(height: 10),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: const Row(
                  children: [
                    Icon(Icons.keyboard, size: 16, color: Color(0xFF94A3B8)),
                    SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        'To activate: Press Win + P on Windows and select "Extend".',
                        style: TextStyle(color: Color(0xFFCBD5E1), fontSize: 12),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildControlsSection(ThemeData theme) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF1E293B),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF334155)),
      ),
      child: Column(
        children: [
          _buildGestureRow(Icons.touch_app, 'Single Tap', 'Standard left-click on desktop elements'),
          const Divider(color: Color(0xFF334155), height: 16),
          _buildGestureRow(Icons.double_arrow, 'Double Tap', 'Double-click to open files or folders'),
          const Divider(color: Color(0xFF334155), height: 16),
          _buildGestureRow(Icons.fingerprint, 'Long Press (500ms)', 'Context menu / right-click action'),
          const Divider(color: Color(0xFF334155), height: 16),
          _buildGestureRow(Icons.pan_tool, 'Pan / Drag', 'Move the mouse pointer or drag desktop windows'),
          const Divider(color: Color(0xFF334155), height: 16),
          _buildGestureRow(Icons.swap_vert, 'Two-Finger Drag', 'Smooth vertical mouse wheel scrolling'),
          const Divider(color: Color(0xFF334155), height: 16),
          _buildGestureRow(Icons.keyboard, 'Keyboard Toolbar', 'Tap keyboard icon to access Ctrl, Alt, Win, Esc, and text input'),
          const Divider(color: Color(0xFF334155), height: 16),
          _buildGestureRow(Icons.power_settings_new, 'Screen Sleep Recovery', 'If your phone locks, simply unlock and reopen DuetView—it recovers automatically!'),
        ],
      ),
    );
  }

  Widget _buildTroubleshootingSection(ThemeData theme) {
    return Material(
      color: const Color(0xFF1E293B),
      clipBehavior: Clip.antiAlias,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: Color(0xFF334155)),
      ),
      child: Column(
        children: [
          _buildFaqTile(
            'QR code fails to scan or camera is dark',
            'Ensure you granted Camera permissions in Android Settings > Apps > DuetView. Hold the phone steady about 10-15 inches from your monitor. If lighting is poor, or you prefer typing, tap "Manual IP" to connect via address.',
          ),
          const Divider(color: Color(0xFF334155), height: 1),
          _buildFaqTile(
            'Cannot connect or "Connection Refused"',
            '1. Confirm DuetView PC has "▶ Start Host Service" enabled.\n'
            '2. Check Windows Firewall: allow DuetView through private networks (ports 52025 and 52027).\n'
            '3. Confirm both devices are connected to the same Wi-Fi router subnet.\n'
            '4. Disable "AP Isolation" or "Guest Network Isolation" in your router settings if enabled.',
          ),
          const Divider(color: Color(0xFF334155), height: 1),
          _buildFaqTile(
            'Pairing token expired',
            'Pairing tokens automatically expire after 5 minutes for security. Simply click "🔄 Regenerate QR" in the DuetView PC window and scan the refreshed code.',
          ),
          const Divider(color: Color(0xFF334155), height: 1),
          _buildFaqTile(
            'Streaming is stuttering or delayed',
            'Connect both devices to a 5 GHz Wi-Fi band instead of 2.4 GHz, or select a lower bitrate profile (e.g. 2000 Kbps) in DuetView PC settings. For zero wireless interference, use USB Tethering mode.',
          ),
          const Divider(color: Color(0xFF334155), height: 1),
          _buildFaqTile(
            'Virtual display driver not installed',
            'Desktop Mirroring works immediately without any driver! If you want an independent second screen (Extend mode), click "Install Virtual Display Driver" in DuetView PC and approve the Windows UAC elevation prompt.',
          ),
        ],
      ),
    );
  }

  Widget _buildStepItem(int step, String text) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 24,
            height: 24,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              color: const Color(0xFF38BDF8).withValues(alpha: 0.15),
              shape: BoxShape.circle,
              border: Border.all(color: const Color(0xFF38BDF8).withValues(alpha: 0.4)),
            ),
            child: Text(
              '$step',
              style: const TextStyle(
                fontSize: 11,
                fontWeight: FontWeight.bold,
                color: Color(0xFF38BDF8),
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              text,
              style: const TextStyle(
                fontSize: 13,
                color: Color(0xFFCBD5E1),
                height: 1.4,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildGestureRow(IconData icon, String title, String description) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Icon(icon, size: 20, color: const Color(0xFF38BDF8)),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                title,
                style: const TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 13,
                  color: Colors.white,
                ),
              ),
              const SizedBox(height: 2),
              Text(
                description,
                style: const TextStyle(
                  fontSize: 12,
                  color: Color(0xFF94A3B8),
                  height: 1.3,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildFaqTile(String question, String answer) {
    return ExpansionTile(
      iconColor: const Color(0xFF38BDF8),
      collapsedIconColor: const Color(0xFF94A3B8),
      title: Text(
        question,
        style: const TextStyle(
          fontSize: 13,
          fontWeight: FontWeight.w600,
          color: Colors.white,
        ),
      ),
      childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 14),
      children: [
        Text(
          answer,
          style: const TextStyle(
            fontSize: 12,
            color: Color(0xFF94A3B8),
            height: 1.4,
          ),
        ),
      ],
    );
  }

  Widget _buildPcDownloadBanner(BuildContext context, ThemeData theme) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF1E293B),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF38BDF8).withValues(alpha: 0.35)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: const Color(0xFF38BDF8).withValues(alpha: 0.15),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(
                  Icons.laptop_windows_rounded,
                  color: Color(0xFF38BDF8),
                  size: 22,
                ),
              ),
              const SizedBox(width: 12),
              const Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'DuetView PC Companion Required',
                      style: TextStyle(
                        fontSize: 14.5,
                        fontWeight: FontWeight.bold,
                        color: Colors.white,
                      ),
                    ),
                    SizedBox(height: 2),
                    Text(
                      'Download DuetView PC on Windows to enable streaming',
                      style: TextStyle(
                        fontSize: 12,
                        color: Color(0xFF94A3B8),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 14),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () => DownloadPcGuideSheet.show(context),
                  icon: const Icon(Icons.help_outline, size: 16),
                  label: const Text('View Setup Steps', style: TextStyle(fontSize: 12.5)),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: const Color(0xFF38BDF8),
                    side: const BorderSide(color: Color(0xFF38BDF8)),
                    padding: const EdgeInsets.symmetric(vertical: 10),
                  ),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: () => UrlLauncherService.openUrl(
                    context,
                    UrlLauncherService.githubReleases,
                    title: 'GitHub Releases',
                  ),
                  icon: const Icon(Icons.open_in_new, size: 16),
                  label: const Text('Download PC', style: TextStyle(fontSize: 12.5)),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFF0284C7),
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(vertical: 10),
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
