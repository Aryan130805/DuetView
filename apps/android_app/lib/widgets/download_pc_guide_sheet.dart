import 'package:flutter/material.dart';

import '../services/url_launcher_service.dart';
import '../theme/app_theme.dart';
import 'responsive_container.dart';

/// Modal guide providing a step-by-step walkthrough for downloading,
/// installing, and launching the DuetView PC companion application on Windows.
class DownloadPcGuideSheet extends StatelessWidget {
  final ScrollController? scrollController;

  const DownloadPcGuideSheet({super.key, this.scrollController});

  /// Presents the guide as an adaptive modal bottom sheet on phones
  /// or a centered dialog on wider tablet viewports.
  static Future<void> show(BuildContext context) {
    if (context.isExpanded) {
      return showDialog(
        context: context,
        builder: (ctx) => Dialog(
          backgroundColor: AppTheme.secondaryBackground,
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 580, maxHeight: 720),
            child: const DownloadPcGuideSheet(),
          ),
        ),
      );
    }

    return showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: AppTheme.secondaryBackground,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      builder: (ctx) => DraggableScrollableSheet(
        initialChildSize: 0.85,
        minChildSize: 0.5,
        maxChildSize: 0.95,
        expand: false,
        builder: (ctx, scrollController) => DownloadPcGuideSheet(
          scrollController: scrollController,
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        // Drag handle / Sheet header
        Padding(
          padding: const EdgeInsets.only(top: 12, bottom: 8),
          child: Container(
            width: 44,
            height: 4,
            decoration: BoxDecoration(
              color: AppTheme.border.withValues(alpha: 0.8),
              borderRadius: BorderRadius.circular(2),
            ),
          ),
        ),

        // Scrollable content
        Expanded(
          child: SingleChildScrollView(
            controller: scrollController,
            padding: const EdgeInsets.fromLTRB(20, 8, 20, 16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Header banner
                _buildHeaderBanner(context),
                const SizedBox(height: 20),

                // System Requirements callout
                _buildRequirementsBanner(),
                const SizedBox(height: 20),

                // Step-by-step guide
                const Text(
                  'Quick Setup Guide',
                  style: TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                    color: AppTheme.primaryText,
                    letterSpacing: -0.3,
                  ),
                ),
                const SizedBox(height: 12),

                _buildStep(
                  stepNumber: 1,
                  title: 'Download the Release',
                  description:
                      'Open the GitHub Releases page and download the installer (DuetView-PC-Setup-v1.0.0.exe) or portable ZIP.',
                  icon: Icons.download_rounded,
                ),
                _buildStep(
                  stepNumber: 2,
                  title: 'Install or Extract',
                  description:
                      'Run the setup wizard or extract the portable build to your chosen folder on Windows.',
                  icon: Icons.folder_zip_rounded,
                ),
                _buildStep(
                  stepNumber: 3,
                  title: 'Launch DuetView PC',
                  description:
                      'Start DuetView PC on your computer. It starts the local streaming service and displays your PC IP and pairing QR.',
                  icon: Icons.desktop_windows_rounded,
                ),
                _buildStep(
                  stepNumber: 4,
                  title: 'Install Virtual Display Driver (Optional)',
                  description:
                      'In DuetView PC, click "Install Virtual Display Driver" to enable true extended desktop display mode.',
                  icon: Icons.extension_rounded,
                ),
                _buildStep(
                  stepNumber: 5,
                  title: 'Connect & Extend',
                  description:
                      'Use Discover PCs, Scan QR, or USB mode in this Android app to connect immediately.',
                  icon: Icons.wifi_tethering_rounded,
                  isLast: true,
                ),
                const SizedBox(height: 16),
              ],
            ),
          ),
        ),

        // Bottom action bar
        _buildBottomActions(context),
      ],
    );
  }

  Widget _buildHeaderBanner(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          colors: [Color(0xFF132847), Color(0xFF0F1E38)],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF264F7C)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 48,
            height: 48,
            decoration: BoxDecoration(
              gradient: const LinearGradient(
                colors: [AppTheme.primaryBlue, AppTheme.violetAccent],
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
              ),
              borderRadius: BorderRadius.circular(12),
              boxShadow: [
                BoxShadow(
                  color: AppTheme.primaryBlue.withValues(alpha: 0.35),
                  blurRadius: 10,
                  offset: const Offset(0, 3),
                ),
              ],
            ),
            child: const Icon(
              Icons.laptop_windows_rounded,
              color: Colors.white,
              size: 26,
            ),
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Expanded(
                      child: Text(
                        'DuetView PC',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                          color: AppTheme.primaryText,
                          letterSpacing: -0.3,
                        ),
                      ),
                    ),
                    Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 8,
                        vertical: 3,
                      ),
                      decoration: BoxDecoration(
                        color: AppTheme.primaryBlue.withValues(alpha: 0.2),
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(
                          color: AppTheme.primaryBlue.withValues(alpha: 0.4),
                        ),
                      ),
                      child: const Text(
                        'v1.0.0',
                        style: TextStyle(
                          fontSize: 11,
                          fontWeight: FontWeight.w600,
                          color: AppTheme.cyanAccent,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 4),
                const Text(
                  'DuetView PC runs on your Windows computer and allows your Android device to connect as a high-performance second monitor.',
                  style: TextStyle(
                    fontSize: 12.5,
                    color: AppTheme.secondaryText,
                    height: 1.35,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildRequirementsBanner() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: AppTheme.primaryCard.withValues(alpha: 0.6),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: AppTheme.border.withValues(alpha: 0.6)),
      ),
      child: const Row(
        children: [
          Icon(Icons.info_outline_rounded, color: AppTheme.cyanAccent, size: 18),
          SizedBox(width: 10),
          Expanded(
            child: Text(
              'Windows 10 / 11 (64-bit) • Direct Wi-Fi or USB • 100% Free & Open Source',
              style: TextStyle(
                fontSize: 11.5,
                color: AppTheme.secondaryText,
                height: 1.3,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildStep({
    required int stepNumber,
    required String title,
    required String description,
    required IconData icon,
    bool isLast = false,
  }) {
    return IntrinsicHeight(
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Column(
            children: [
              Container(
                width: 32,
                height: 32,
                decoration: BoxDecoration(
                  color: AppTheme.elevatedCard,
                  shape: BoxShape.circle,
                  border: Border.all(
                    color: AppTheme.primaryBlue.withValues(alpha: 0.6),
                  ),
                ),
                child: Center(
                  child: Text(
                    '$stepNumber',
                    style: const TextStyle(
                      fontSize: 13,
                      fontWeight: FontWeight.bold,
                      color: AppTheme.cyanAccent,
                    ),
                  ),
                ),
              ),
              if (!isLast)
                Expanded(
                  child: Container(
                    width: 2,
                    color: AppTheme.border.withValues(alpha: 0.5),
                  ),
                ),
            ],
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Padding(
              padding: EdgeInsets.only(bottom: isLast ? 0 : 18),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Icon(icon, size: 16, color: AppTheme.primaryBlue),
                      const SizedBox(width: 6),
                      Expanded(
                        child: Text(
                          title,
                          style: const TextStyle(
                            fontSize: 14,
                            fontWeight: FontWeight.w600,
                            color: AppTheme.primaryText,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 4),
                  Text(
                    description,
                    style: const TextStyle(
                      fontSize: 12,
                      color: AppTheme.secondaryText,
                      height: 1.35,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildBottomActions(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(20, 12, 20, 24),
      decoration: BoxDecoration(
        color: AppTheme.mainBackground,
        border: Border(top: BorderSide(color: AppTheme.border.withValues(alpha: 0.6))),
      ),
      child: SafeArea(
        top: false,
        child: Row(
          children: [
            Expanded(
              child: OutlinedButton(
                onPressed: () => Navigator.pop(context),
                style: OutlinedButton.styleFrom(
                  foregroundColor: AppTheme.secondaryText,
                  side: BorderSide(color: AppTheme.border.withValues(alpha: 0.8)),
                  padding: const EdgeInsets.symmetric(vertical: 14),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                ),
                child: const Text('Close'),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              flex: 2,
              child: Container(
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                    colors: [Color(0xFF2563EB), Color(0xFF1D4ED8)],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                  borderRadius: BorderRadius.circular(12),
                  boxShadow: [
                    BoxShadow(
                      color: AppTheme.primaryBlue.withValues(alpha: 0.35),
                      blurRadius: 10,
                      offset: const Offset(0, 3),
                    ),
                  ],
                ),
                child: ElevatedButton.icon(
                  onPressed: () => UrlLauncherService.openUrl(
                    context,
                    UrlLauncherService.githubReleases,
                    title: 'GitHub Releases',
                  ),
                  icon: const Icon(Icons.open_in_new_rounded, size: 18),
                  label: const Text(
                    'Open GitHub Releases',
                    style: TextStyle(fontWeight: FontWeight.w600),
                  ),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.transparent,
                    foregroundColor: Colors.white,
                    shadowColor: Colors.transparent,
                    padding: const EdgeInsets.symmetric(vertical: 14),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
