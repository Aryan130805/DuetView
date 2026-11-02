import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/saved_pc.dart';
import '../services/connection_service.dart';
import '../services/decoder_bridge.dart';
import '../services/pc_health_service.dart';
import '../services/saved_pc_storage.dart';
import '../services/settings_service.dart';
import '../theme/app_theme.dart';
import '../widgets/connection_status_card.dart';
import '../widgets/responsive_container.dart';
import '../widgets/saved_pc_card.dart';
import 'connection_screen.dart';
import 'guide_screen.dart';
import 'qr_scanner_screen.dart';
import 'settings_screen.dart';
import 'streaming_screen.dart';

class HomeScreen extends StatefulWidget {
  final ConnectionService connectionService;
  final SettingsService settingsService;
  final SecondScreenDecoderBridge? decoderBridge;
  final SavedPcStorage? savedPcStorage;
  final PcHealthService? pcHealthService;

  const HomeScreen({
    super.key,
    required this.connectionService,
    required this.settingsService,
    this.decoderBridge,
    this.savedPcStorage,
    this.pcHealthService,
  });

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late final SavedPcStorage _storage;
  bool _showIntroCard = false;
  int _currentTabIndex = 0;

  @override
  void initState() {
    super.initState();
    _storage = widget.savedPcStorage ?? SavedPcStorage();
    if (widget.savedPcStorage == null) {
      _storage.load();
    }
    _loadIntroStatus();
  }

  Future<void> _loadIntroStatus() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final dismissed = prefs.getBool('duetview_intro_dismissed') ?? false;
      if (mounted) {
        setState(() {
          _showIntroCard = !dismissed;
        });
      }
    } catch (_) {}
  }

  Future<void> _dismissIntro() async {
    setState(() {
      _showIntroCard = false;
    });
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setBool('duetview_intro_dismissed', true);
    } catch (_) {}
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
          connectionService: widget.connectionService,
          settingsService: widget.settingsService,
          savedPcStorage: _storage,
          autoStartDiscovery: autoStartDiscovery,
          initialTab: manualOnly ? 1 : 0,
        ),
      ),
    );
  }

  Future<void> _openQrScanner(BuildContext context) async {
    final pairedPc = await Navigator.push<SavedPc>(
      context,
      MaterialPageRoute(
        builder: (context) => QrScannerScreen(
          storage: _storage,
          connectionService: widget.connectionService,
          settingsService: widget.settingsService,
          decoderBridge:
              widget.decoderBridge ?? widget.connectionService.decoderBridge,
        ),
      ),
    );

    if (pairedPc != null && mounted && context.mounted) {
      if (widget.connectionService.state.isStreaming ||
          widget.connectionService.state.isConnected) {
        _openStreamingScreen(context);
      } else {
        await _connectToSavedPc(context, pairedPc);
      }
    }
  }

  void _openGuideScreen(BuildContext context) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => const GuideScreen(),
      ),
    );
  }

  void _openStreamingScreen(BuildContext context) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => StreamingScreen(
          connectionService: widget.connectionService,
          settingsService: widget.settingsService,
          decoderBridge:
              widget.decoderBridge ?? widget.connectionService.decoderBridge,
        ),
      ),
    ).then((_) {
      widget.pcHealthService?.setActivelyStreamingPc(null);
    });
  }

  Future<void> _connectToSavedPc(BuildContext context, SavedPc pc) async {
    if (pc.endpoints.isEmpty) return;

    widget.pcHealthService?.setActivelyStreamingPc(pc.id);

    bool success = false;
    String? workingEndpoint;

    for (final endpoint in pc.endpoints) {
      final ok = await widget.connectionService.connectToDevice(
        endpoint,
        pc.port,
        pcName: pc.name,
      );
      if (ok) {
        success = true;
        workingEndpoint = endpoint;
        break;
      }
    }

    if (success && workingEndpoint != null) {
      await _storage.updatePcStatus(
        pc.id,
        PcOnlineStatus.streaming,
        reachableEndpoint: workingEndpoint,
      );

      if (mounted && context.mounted) {
        _openStreamingScreen(context);
      }
    } else {
      widget.pcHealthService?.setActivelyStreamingPc(null);
      await _storage.updatePcStatus(pc.id, PcOnlineStatus.offline);

      if (mounted && context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              'Could not connect to ${pc.name} on ${pc.endpoints.join(', ')}:${pc.port}. '
              'Ensure DuetView PC is running on your computer.',
            ),
            backgroundColor: AppTheme.error,
            duration: const Duration(seconds: 4),
          ),
        );
      }
    }
  }

  void _onBottomNavTapped(int index) {
    setState(() {
      _currentTabIndex = index;
    });

    if (index == 1) {
      // Display Tab
      if (widget.connectionService.state.isStreaming ||
          widget.connectionService.state.isConnected) {
        _openStreamingScreen(context);
      } else {
        _showDisplayModesSheet(context);
      }
    } else if (index == 2) {
      // Tools Tab
      _showToolsSheet(context);
    }
  }

  void _showDisplayModesSheet(BuildContext context) {
    showModalBottomSheet(
      context: context,
      backgroundColor: AppTheme.secondaryBackground,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) => Padding(
        padding: const EdgeInsets.fromLTRB(20, 20, 20, 32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Center(
              child: Container(
                width: 40,
                height: 4,
                decoration: BoxDecoration(
                  color: AppTheme.border,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
            ),
            const SizedBox(height: 16),
            const Text(
              'Display Modes',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
                color: AppTheme.primaryText,
              ),
            ),
            const SizedBox(height: 8),
            const Text(
              'DuetView turns this Android device into an extended virtual monitor or a high-fps mirror of your Windows desktop.',
              style: TextStyle(color: AppTheme.secondaryText, fontSize: 13),
            ),
            const SizedBox(height: 16),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(Icons.cast_connected, color: AppTheme.cyanAccent),
              ),
              title: const Text('Connect to PC', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
              subtitle: const Text('Start streaming secondary display', style: TextStyle(color: AppTheme.mutedText)),
              trailing: const Icon(Icons.arrow_forward_ios, size: 14, color: AppTheme.mutedText),
              onTap: () {
                Navigator.pop(ctx);
                _openConnectScreen(context);
              },
            ),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(Icons.settings_display, color: AppTheme.primaryBlue),
              ),
              title: const Text('Display Preferences', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
              subtitle: const Text('Resolution, 60/90/120 FPS, and scaling', style: TextStyle(color: AppTheme.mutedText)),
              trailing: const Icon(Icons.arrow_forward_ios, size: 14, color: AppTheme.mutedText),
              onTap: () {
                Navigator.pop(ctx);
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (context) => SettingsScreen(settingsService: widget.settingsService),
                  ),
                );
              },
            ),
          ],
        ),
      ),
    );
  }

  void _showToolsSheet(BuildContext context) {
    showModalBottomSheet(
      context: context,
      backgroundColor: AppTheme.secondaryBackground,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) => Padding(
        padding: const EdgeInsets.fromLTRB(20, 20, 20, 32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Center(
              child: Container(
                width: 40,
                height: 4,
                decoration: BoxDecoration(
                  color: AppTheme.border,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
            ),
            const SizedBox(height: 16),
            const Text(
              'Connection & Diagnostic Tools',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
                color: AppTheme.primaryText,
              ),
            ),
            const SizedBox(height: 16),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(Icons.qr_code_scanner, color: AppTheme.violetAccent),
              ),
              title: const Text('QR Code Scanner', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
              subtitle: const Text('Instant pairing via DuetView PC QR', style: TextStyle(color: AppTheme.mutedText)),
              onTap: () {
                Navigator.pop(ctx);
                _openQrScanner(context);
              },
            ),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(Icons.lan_outlined, color: AppTheme.cyanAccent),
              ),
              title: const Text('Manual IP & Port', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
              subtitle: const Text('Connect directly to custom host port', style: TextStyle(color: AppTheme.mutedText)),
              onTap: () {
                Navigator.pop(ctx);
                _openConnectScreen(context, manualOnly: true);
              },
            ),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: const Icon(Icons.usb, color: AppTheme.warning),
              ),
              title: const Text('USB Tethering Guide', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
              subtitle: const Text('Sub-5ms ultra-low latency setup', style: TextStyle(color: AppTheme.mutedText)),
              onTap: () {
                Navigator.pop(ctx);
                _openGuideScreen(context);
              },
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.mainBackground,
      appBar: AppBar(
        backgroundColor: Colors.transparent,
        elevation: 0,
        scrolledUnderElevation: 0,
        title: Row(
          children: [
            Container(
              width: 32,
              height: 32,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(8),
                gradient: const LinearGradient(
                  colors: [Color(0xFF2688FF), Color(0xFF1445A8)],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
                boxShadow: [
                  BoxShadow(
                    color: AppTheme.primaryBlue.withValues(alpha: 0.35),
                    blurRadius: 8,
                    offset: const Offset(0, 2),
                  ),
                ],
              ),
              padding: const EdgeInsets.all(3),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(6),
                child: Image.asset(
                  'assets/images/duetview_icon.png',
                  errorBuilder: (context, error, stackTrace) =>
                      const Icon(Icons.desktop_windows, size: 20, color: Colors.white),
                ),
              ),
            ),
            const SizedBox(width: 12),
            const Text(
              'DuetView',
              style: TextStyle(
                fontWeight: FontWeight.bold,
                fontSize: 20,
                color: Colors.white,
                letterSpacing: -0.5,
              ),
            ),
          ],
        ),
        actions: [
          // Circular Help Button
          Container(
            margin: const EdgeInsets.only(right: 8),
            decoration: BoxDecoration(
              color: AppTheme.elevatedCard.withValues(alpha: 0.8),
              shape: BoxShape.circle,
              border: Border.all(color: AppTheme.border.withValues(alpha: 0.6)),
            ),
            child: IconButton(
              icon: const Icon(Icons.help_outline, color: AppTheme.secondaryText, size: 20),
              tooltip: 'How to Use DuetView',
              onPressed: () => _openGuideScreen(context),
            ),
          ),
          // Circular Settings Button
          Container(
            margin: const EdgeInsets.only(right: 16),
            decoration: BoxDecoration(
              color: AppTheme.elevatedCard.withValues(alpha: 0.8),
              shape: BoxShape.circle,
              border: Border.all(color: AppTheme.border.withValues(alpha: 0.6)),
            ),
            child: IconButton(
              icon: const Icon(Icons.settings_outlined, color: AppTheme.secondaryText, size: 20),
              tooltip: 'Settings',
              onPressed: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (context) => SettingsScreen(settingsService: widget.settingsService),
                  ),
                );
              },
            ),
          ),
        ],
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.symmetric(horizontal: 18),
          child: ResponsiveContainer(
            child: ListenableBuilder(
              listenable: widget.connectionService,
              builder: (context, _) {
                final state = widget.connectionService.state;

                return Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const SizedBox(height: 12),

                    // First-Launch banner if active
                    _buildFirstLaunchBanner(),

                    // Hero Banner: Title, Subtitle, and Dual-Screen Graphic
                    _buildHeroSection(),
                    const SizedBox(height: 20),

                    // Connection Status Card
                    ConnectionStatusCard(
                      state: state,
                      onCancel: () => widget.connectionService.cancelConnection(),
                      onDisconnect: () => widget.connectionService.disconnect(),
                      onOpenStream: () => _openStreamingScreen(context),
                    ),
                    const SizedBox(height: 18),

                    // Primary Action Cards (Side-by-side: Discover PCs & Scan to Connect)
                    _buildPrimaryActionCards(context),
                    const SizedBox(height: 12),

                    // Secondary Action Buttons (Side-by-side: Manual IP & How to Use)
                    _buildSecondaryActionButtons(context),
                    const SizedBox(height: 24),

                    // Saved PCs Section
                    _buildSavedPcsSection(context),
                    const SizedBox(height: 24),

                    // Network Requirements Advisory Card
                    _buildNetworkAdvisoryCard(context),
                    const SizedBox(height: 28),
                  ],
                );
              },
            ),
          ),
        ),
      ),
      bottomNavigationBar: _buildBottomNavigationBar(),
    );
  }

  Widget _buildHeroSection() {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Left: Hero Copy
          Expanded(
            flex: 6,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                ShaderMask(
                  shaderCallback: (bounds) => const LinearGradient(
                    colors: [Colors.white, AppTheme.cyanAccent, Colors.white],
                    stops: [0.0, 0.6, 1.0],
                  ).createShader(bounds),
                  child: const Text(
                    'Turn your Android device into a second display for your PC.',
                    style: TextStyle(
                      fontSize: 19,
                      fontWeight: FontWeight.w700,
                      color: Colors.white,
                      height: 1.25,
                      letterSpacing: -0.4,
                    ),
                  ),
                ),
                const SizedBox(height: 8),
                const Text(
                  'Extend, work, and multitask seamlessly over Wi-Fi or USB. Free, local, and private.',
                  style: TextStyle(
                    fontSize: 12.5,
                    color: AppTheme.secondaryText,
                    height: 1.4,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          // Right: Dual-Display Vector Illustration (Laptop + Tablet)
          Expanded(
            flex: 4,
            child: _buildDualScreenGraphic(),
          ),
        ],
      ),
    );
  }

  Widget _buildDualScreenGraphic() {
    return SizedBox(
      height: 90,
      child: Stack(
        alignment: Alignment.centerRight,
        children: [
          // Laptop Screen (Back)
          Positioned(
            left: 0,
            top: 6,
            child: Container(
              width: 82,
              height: 56,
              decoration: BoxDecoration(
                color: const Color(0xFF0F1B30),
                borderRadius: BorderRadius.circular(6),
                border: Border.all(color: const Color(0xFF264670), width: 1.5),
                boxShadow: [
                  BoxShadow(
                    color: AppTheme.primaryBlue.withValues(alpha: 0.2),
                    blurRadius: 10,
                  ),
                ],
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(4),
                child: Container(
                  decoration: const BoxDecoration(
                    gradient: LinearGradient(
                      colors: [Color(0xFF1B2F6E), Color(0xFF0E1A38)],
                      begin: Alignment.topLeft,
                      end: Alignment.bottomRight,
                    ),
                  ),
                  child: Center(
                    child: Container(
                      width: 32,
                      height: 4,
                      decoration: BoxDecoration(
                        color: AppTheme.cyanAccent.withValues(alpha: 0.6),
                        borderRadius: BorderRadius.circular(2),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
          // Laptop Base
          Positioned(
            left: 0,
            top: 61,
            child: Container(
              width: 90,
              height: 6,
              decoration: BoxDecoration(
                color: const Color(0xFF1E2D44),
                borderRadius: const BorderRadius.vertical(bottom: Radius.circular(4)),
                border: Border.all(color: const Color(0xFF2B3F5D)),
              ),
            ),
          ),
          // Tablet Screen (Front overlay)
          Positioned(
            right: 0,
            bottom: 6,
            child: Container(
              width: 58,
              height: 48,
              decoration: BoxDecoration(
                color: const Color(0xFF18182E),
                borderRadius: BorderRadius.circular(6),
                border: Border.all(color: const Color(0xFF5A448E), width: 1.5),
                boxShadow: [
                  BoxShadow(
                    color: AppTheme.violetAccent.withValues(alpha: 0.35),
                    blurRadius: 12,
                    offset: const Offset(-2, 4),
                  ),
                ],
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(4),
                child: Container(
                  decoration: const BoxDecoration(
                    gradient: LinearGradient(
                      colors: [Color(0xFF381F66), Color(0xFF1B1638)],
                      begin: Alignment.topRight,
                      end: Alignment.bottomLeft,
                    ),
                  ),
                  child: Center(
                    child: Icon(
                      Icons.cast_connected,
                      size: 16,
                      color: AppTheme.cyanAccent.withValues(alpha: 0.85),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildPrimaryActionCards(BuildContext context) {
    return Row(
      children: [
        // Discover PCs Card (Cyan Accent)
        Expanded(
          child: Material(
            color: Colors.transparent,
            borderRadius: BorderRadius.circular(16),
            child: InkWell(
              onTap: () => _openConnectScreen(
                context,
                autoStartDiscovery: true,
                manualOnly: false,
              ),
              borderRadius: BorderRadius.circular(16),
              child: Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                    colors: [Color(0xFF132847), Color(0xFF0F1E38)],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: const Color(0xFF264F7C)),
                  boxShadow: [
                    BoxShadow(
                      color: AppTheme.primaryBlue.withValues(alpha: 0.15),
                      blurRadius: 12,
                      offset: const Offset(0, 4),
                    ),
                  ],
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Container(
                      width: 42,
                      height: 42,
                      decoration: BoxDecoration(
                        color: const Color(0xFF193B63),
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: const Color(0xFF2F6396)),
                      ),
                      child: const Icon(
                        Icons.sensors_rounded,
                        color: AppTheme.cyanAccent,
                        size: 22,
                      ),
                    ),
                    const SizedBox(height: 14),
                    const Text(
                      'Discover PCs',
                      style: TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                        color: AppTheme.primaryText,
                      ),
                    ),
                    const SizedBox(height: 4),
                    const Text(
                      'Find DuetView PC on\nyour network',
                      style: TextStyle(
                        fontSize: 12,
                        color: AppTheme.secondaryText,
                        height: 1.3,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
        const SizedBox(width: 14),
        // Scan to Connect Card (Violet Accent)
        Expanded(
          child: Material(
            color: Colors.transparent,
            borderRadius: BorderRadius.circular(16),
            child: InkWell(
              onTap: () => _openQrScanner(context),
              borderRadius: BorderRadius.circular(16),
              child: Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                    colors: [Color(0xFF2A1C46), Color(0xFF191330)],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: const Color(0xFF4C3078)),
                  boxShadow: [
                    BoxShadow(
                      color: AppTheme.violetAccent.withValues(alpha: 0.15),
                      blurRadius: 12,
                      offset: const Offset(0, 4),
                    ),
                  ],
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Container(
                      width: 42,
                      height: 42,
                      decoration: BoxDecoration(
                        color: const Color(0xFF3B235E),
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: const Color(0xFF643D9B)),
                      ),
                      child: const Icon(
                        Icons.qr_code_scanner_rounded,
                        color: Color(0xFFB57AFF),
                        size: 22,
                      ),
                    ),
                    const SizedBox(height: 14),
                    const Text(
                      'Scan to Connect',
                      style: TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                        color: AppTheme.primaryText,
                      ),
                    ),
                    const SizedBox(height: 4),
                    const Text(
                      'Scan QR from\nDuetView PC',
                      style: TextStyle(
                        fontSize: 12,
                        color: AppTheme.secondaryText,
                        height: 1.3,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildSecondaryActionButtons(BuildContext context) {
    return Row(
      children: [
        // Manual IP Button
        Expanded(
          child: Material(
            color: Colors.transparent,
            child: InkWell(
              onTap: () => _openConnectScreen(context, manualOnly: true),
              borderRadius: BorderRadius.circular(12),
              child: Container(
                padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 14),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: AppTheme.border),
                ),
                child: const Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.lan_outlined, size: 18, color: AppTheme.secondaryText),
                    SizedBox(width: 8),
                    Text(
                      'Manual IP',
                      style: TextStyle(
                        fontSize: 13.5,
                        fontWeight: FontWeight.w600,
                        color: AppTheme.primaryText,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
        const SizedBox(width: 12),
        // How to Use Button
        Expanded(
          child: Material(
            color: Colors.transparent,
            child: InkWell(
              onTap: () => _openGuideScreen(context),
              borderRadius: BorderRadius.circular(12),
              child: Container(
                padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 14),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard,
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: AppTheme.border),
                ),
                child: const Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.menu_book_outlined, size: 18, color: AppTheme.secondaryText),
                    SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        'How to Use',
                        style: TextStyle(
                          fontSize: 13.5,
                          fontWeight: FontWeight.w600,
                          color: AppTheme.primaryText,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                    Icon(Icons.chevron_right_rounded, size: 18, color: AppTheme.mutedText),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildSavedPcsSection(BuildContext context) {
    return ListenableBuilder(
      listenable: _storage,
      builder: (context, _) {
        final pcs = _storage.savedPcs;

        return Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  children: [
                    const Icon(
                      Icons.bookmark_outline_rounded,
                      size: 20,
                      color: AppTheme.cyanAccent,
                    ),
                    const SizedBox(width: 8),
                    Text(
                      'Saved PCs (${pcs.length})',
                      style: const TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                        color: AppTheme.primaryText,
                      ),
                    ),
                  ],
                ),
                // + Add PC Pill Button
                Material(
                  color: Colors.transparent,
                  child: InkWell(
                    onTap: () => _openQrScanner(context),
                    borderRadius: BorderRadius.circular(20),
                    child: Container(
                      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
                      decoration: BoxDecoration(
                        color: const Color(0xFF132542),
                        borderRadius: BorderRadius.circular(20),
                        border: Border.all(color: const Color(0xFF264C7A)),
                      ),
                      child: const Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Text(
                            'Add PC',
                            style: TextStyle(
                              fontSize: 12.5,
                              fontWeight: FontWeight.w600,
                              color: AppTheme.cyanAccent,
                            ),
                          ),
                          SizedBox(width: 4),
                          Icon(Icons.add, size: 15, color: AppTheme.cyanAccent),
                        ],
                      ),
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),

            if (pcs.isEmpty)
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(22),
                decoration: BoxDecoration(
                  color: AppTheme.primaryCard.withValues(alpha: 0.6),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: AppTheme.border.withValues(alpha: 0.6)),
                ),
                child: const Column(
                  children: [
                    Icon(Icons.desktop_windows_outlined, size: 36, color: AppTheme.mutedText),
                    SizedBox(height: 10),
                    Text(
                      'No saved computers yet',
                      style: TextStyle(
                        color: AppTheme.primaryText,
                        fontWeight: FontWeight.w600,
                        fontSize: 14,
                      ),
                    ),
                    SizedBox(height: 4),
                    Text(
                      'Tap "Scan to Connect" above to scan the pairing QR code displayed by DuetView PC.',
                      style: TextStyle(color: AppTheme.mutedText, fontSize: 12),
                      textAlign: TextAlign.center,
                    ),
                  ],
                ),
              )
            else
              ...pcs.map(
                (pc) => SavedPcCard(
                  pc: pc,
                  onConnect: () => _connectToSavedPc(context, pc),
                  onRename: (newName) => _storage.updatePcName(pc.id, newName),
                  onRemove: () => _storage.removePc(pc.id),
                ),
              ),
          ],
        );
      },
    );
  }

  Widget _buildNetworkAdvisoryCard(BuildContext context) {
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: () => _openGuideScreen(context),
        borderRadius: BorderRadius.circular(16),
        child: Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            color: AppTheme.primaryCard.withValues(alpha: 0.7),
            borderRadius: BorderRadius.circular(16),
            border: Border.all(color: AppTheme.border),
          ),
          child: Row(
            children: [
              Container(
                width: 44,
                height: 44,
                decoration: BoxDecoration(
                  color: AppTheme.elevatedCard,
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: AppTheme.border),
                ),
                child: const Icon(
                  Icons.wifi_rounded,
                  color: AppTheme.cyanAccent,
                  size: 22,
                ),
              ),
              const SizedBox(width: 14),
              const Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'Network Requirements',
                      style: TextStyle(
                        fontWeight: FontWeight.w700,
                        fontSize: 14.5,
                        color: AppTheme.primaryText,
                      ),
                    ),
                    SizedBox(height: 3),
                    Text(
                      'Use a stable 5 GHz Wi-Fi (same network) or a USB tethered connection for best performance.',
                      style: TextStyle(
                        color: AppTheme.secondaryText,
                        fontSize: 12,
                        height: 1.35,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 8),
              const Icon(
                Icons.chevron_right_rounded,
                color: AppTheme.mutedText,
                size: 22,
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildBottomNavigationBar() {
    return Container(
      decoration: BoxDecoration(
        color: AppTheme.secondaryBackground,
        border: Border(
          top: BorderSide(color: AppTheme.border.withValues(alpha: 0.7)),
        ),
      ),
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 24),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceAround,
        children: [
          _buildNavItem(
            index: 0,
            icon: Icons.home_rounded,
            label: 'Home',
          ),
          _buildNavItem(
            index: 1,
            icon: Icons.desktop_windows_rounded,
            label: 'Display',
          ),
          _buildNavItem(
            index: 2,
            icon: Icons.grid_view_rounded,
            label: 'Tools',
          ),
        ],
      ),
    );
  }

  Widget _buildNavItem({
    required int index,
    required IconData icon,
    required String label,
  }) {
    final isSelected = _currentTabIndex == index;

    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: () => _onBottomNavTapped(index),
        borderRadius: BorderRadius.circular(16),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 6),
          decoration: BoxDecoration(
            color: isSelected ? AppTheme.primaryBlue.withValues(alpha: 0.22) : Colors.transparent,
            borderRadius: BorderRadius.circular(16),
            border: isSelected ? Border.all(color: AppTheme.primaryBlue.withValues(alpha: 0.4)) : null,
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                icon,
                color: isSelected ? AppTheme.cyanAccent : AppTheme.secondaryText,
                size: 22,
              ),
              const SizedBox(height: 3),
              Text(
                label,
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.w500,
                  color: isSelected ? AppTheme.cyanAccent : AppTheme.secondaryText,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildFirstLaunchBanner() {
    if (!_showIntroCard) return const SizedBox.shrink();

    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          colors: [Color(0xFF1E293B), Color(0xFF0F172A)],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(
          color: AppTheme.cyanAccent.withValues(alpha: 0.3),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.auto_awesome, color: AppTheme.cyanAccent, size: 20),
              const SizedBox(width: 8),
              const Expanded(
                child: Text(
                  'Getting Started with DuetView',
                  style: TextStyle(
                    fontWeight: FontWeight.bold,
                    color: Colors.white,
                    fontSize: 14,
                  ),
                ),
              ),
              IconButton(
                icon: const Icon(Icons.close, size: 18, color: Colors.white60),
                tooltip: 'Dismiss',
                onPressed: _dismissIntro,
                padding: EdgeInsets.zero,
                constraints: const BoxConstraints(),
              ),
            ],
          ),
          const SizedBox(height: 8),
          const Text(
            '• Turns your phone or tablet into a high-performance Windows PC display.\n'
            '• Tap "Scan to Connect" and point your camera at DuetView PC on your computer.\n'
            '• Connect both devices to the same Wi-Fi network or use USB tethering.\n'
            '• Desktop Mirroring works immediately; Extended Display requires the virtual display driver.',
            style: TextStyle(color: Color(0xFFCBD5E1), fontSize: 12, height: 1.45),
          ),
          const SizedBox(height: 10),
          Row(
            mainAxisAlignment: MainAxisAlignment.end,
            children: [
              TextButton(
                onPressed: () {
                  _dismissIntro();
                  _openGuideScreen(context);
                },
                style: TextButton.styleFrom(
                  foregroundColor: AppTheme.cyanAccent,
                  padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                  visualDensity: VisualDensity.compact,
                ),
                child: const Text('View Full Guide →'),
              ),
              const SizedBox(width: 8),
              TextButton(
                onPressed: _dismissIntro,
                style: TextButton.styleFrom(
                  foregroundColor: const Color(0xFF94A3B8),
                  padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                  visualDensity: VisualDensity.compact,
                ),
                child: const Text('Got it'),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
