import 'package:flutter/material.dart';

import '../models/connection_state.dart';
import '../models/saved_pc.dart';
import '../services/connection_manager.dart';
import '../services/connection_service.dart';
import '../services/discovery_service.dart';
import '../services/pc_health_service.dart';
import '../services/settings_service.dart';
import '../services/saved_pc_storage.dart';
import '../theme/app_theme.dart';
import '../widgets/pc_device_card.dart';
import '../widgets/primary_button.dart';
import '../widgets/responsive_container.dart';
import 'qr_scanner_screen.dart';
import 'streaming_screen.dart';

class ConnectionScreen extends StatefulWidget {
  final ConnectionService connectionService;
  final SettingsService settingsService;
  final SavedPcStorage? savedPcStorage;
  final SharedConnectionManager? connectionManager;
  final PcHealthService? pcHealthService;
  final bool autoStartDiscovery;
  final int initialTab;

  const ConnectionScreen({
    super.key,
    required this.connectionService,
    required this.settingsService,
    this.savedPcStorage,
    this.connectionManager,
    this.pcHealthService,
    this.autoStartDiscovery = false,
    this.initialTab = 0,
  });

  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen> {
  late final DiscoveryService _discoveryService;
  late final TextEditingController _ipController;
  late final TextEditingController _portController;
  late final SharedConnectionManager _connectionManager;
  final _formKey = GlobalKey<FormState>();

  int _selectedTabIndex = 0;

  @override
  void initState() {
    super.initState();
    _discoveryService = DiscoveryService();
    _selectedTabIndex = widget.initialTab;

    _connectionManager = widget.connectionManager ??
        SharedConnectionManager(
          connectionService: widget.connectionService,
          settingsService: widget.settingsService,
          savedPcStorage: widget.savedPcStorage ?? SavedPcStorage(),
          pcHealthService: widget.pcHealthService,
          decoderBridge: widget.connectionService.decoderBridge,
        );

    final initialIp = widget.settingsService.settings.lastHostAddress;
    final initialPort = widget.settingsService.settings.defaultPort;

    _ipController = TextEditingController(text: initialIp);
    _portController = TextEditingController(text: initialPort.toString());

    if (widget.autoStartDiscovery && _selectedTabIndex == 0) {
      _discoveryService.startDiscovery();
    }
  }

  @override
  void dispose() {
    _discoveryService.dispose();
    _ipController.dispose();
    _portController.dispose();
    super.dispose();
  }

  Future<void> _handleConnect({String? host, int? port, String? name}) async {
    final targetHost = host ?? _ipController.text.trim();
    final targetPort =
        port ?? int.tryParse(_portController.text.trim()) ?? 52025;

    await _connectionManager.connect(
      context: context,
      host: targetHost,
      port: targetPort,
      pcName: name,
      replaceCurrentRoute: true,
    );
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Connect to PC'),
        actions: [
          IconButton(
            icon: const Icon(Icons.qr_code_scanner),
            tooltip: 'Scan Pairing QR',
            onPressed: () async {
              final result = await Navigator.push<SavedPc>(
                context,
                MaterialPageRoute(
                  builder: (context) => QrScannerScreen(
                    storage: widget.savedPcStorage ?? SavedPcStorage(),
                    connectionService: widget.connectionService,
                    settingsService: widget.settingsService,
                    decoderBridge: widget.connectionService.decoderBridge,
                  ),
                ),
              );

              if (result != null && mounted) {
                if (widget.connectionService.state.isConnected ||
                    widget.connectionService.state.isStreaming) {
                  if (!context.mounted) return;
                  Navigator.pushReplacement(
                    context,
                    MaterialPageRoute(
                      builder: (context) => StreamingScreen(
                        connectionService: widget.connectionService,
                        settingsService: widget.settingsService,
                        decoderBridge: widget.connectionService.decoderBridge,
                      ),
                    ),
                  );
                }
              }
            },
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          child: ResponsiveContainer(
            child: ListenableBuilder(
              listenable: widget.connectionService,
              builder: (context, _) {
                final connState = widget.connectionService.state;

                return Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    // Segmented control between Discovery and Manual IP
                    SegmentedButton<int>(
                      segments: const [
                        ButtonSegment(
                          value: 0,
                          icon: Icon(Icons.radar, size: 18),
                          label: Text('Discover PCs', overflow: TextOverflow.ellipsis),
                        ),
                        ButtonSegment(
                          value: 1,
                          icon: Icon(Icons.edit_note, size: 18),
                          label: Text('Manual IP', overflow: TextOverflow.ellipsis),
                        ),
                      ],
                      selected: {_selectedTabIndex},
                      onSelectionChanged: (set) {
                        setState(() {
                          _selectedTabIndex = set.first;
                        });
                        if (_selectedTabIndex == 0 &&
                            !_discoveryService.isSearching &&
                            _discoveryService.discoveredDevices.isEmpty) {
                          _discoveryService.startDiscovery();
                        }
                      },
                    ),
                    const SizedBox(height: 20),

                    // In-progress connection banner
                    if (connState.isConnecting) ...[
                      _buildConnectingBanner(theme),
                      const SizedBox(height: 16),
                    ],

                    // Failure alert banner
                    if (connState.isFailed &&
                        connState.errorMessage != null) ...[
                      _buildErrorBanner(theme, connState.errorMessage!),
                      const SizedBox(height: 16),
                    ],

                    // Tab Views
                    if (_selectedTabIndex == 0)
                      _buildDiscoveryView(theme, connState)
                    else
                      _buildManualView(theme, connState),
                  ],
                );
              },
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildConnectingBanner(ThemeData theme) {
    final dev = widget.connectionService.state.connectedDevice;
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: AppTheme.statusConnecting.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: AppTheme.statusConnecting.withValues(alpha: 0.3),
        ),
      ),
      child: Row(
        children: [
          const SizedBox(
            width: 20,
            height: 20,
            child: CircularProgressIndicator(strokeWidth: 2.5),
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Text(
              'Connecting to ${dev?.address ?? "host"}...',
              style: theme.textTheme.bodyMedium?.copyWith(
                fontWeight: FontWeight.w600,
                color: AppTheme.statusConnecting,
              ),
            ),
          ),
          TextButton(
            onPressed: () => widget.connectionService.cancelConnection(),
            child: const Text('Cancel'),
          ),
        ],
      ),
    );
  }

  Widget _buildErrorBanner(ThemeData theme, String message) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: AppTheme.statusFailed.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: AppTheme.statusFailed.withValues(alpha: 0.3)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Icon(
            Icons.error_outline,
            color: AppTheme.statusFailed,
            size: 22,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              message,
              style: theme.textTheme.bodySmall?.copyWith(
                color: AppTheme.statusFailed,
                fontWeight: FontWeight.w500,
                height: 1.35,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildDiscoveryView(ThemeData theme, ConnectionStateModel connState) {
    return ListenableBuilder(
      listenable: _discoveryService,
      builder: (context, _) {
        final isSearching = _discoveryService.isSearching;
        final devices = _discoveryService.discoveredDevices;

        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Expanded(
                  child: Text(
                    'Nearby Windows PCs',
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
                const SizedBox(width: 8),
                if (isSearching)
                  TextButton.icon(
                    onPressed: () => _discoveryService.stopDiscovery(),
                    icon: const Icon(Icons.stop, size: 16),
                    label: const Text('Stop Scan'),
                  )
                else
                  TextButton.icon(
                    onPressed: () => _discoveryService.startDiscovery(),
                    icon: const Icon(Icons.refresh, size: 16),
                    label: const Text('Rescan'),
                  ),
              ],
            ),
            const SizedBox(height: 8),

            if (isSearching) ...[
              const LinearProgressIndicator(),
              const SizedBox(height: 16),
            ],

            if (devices.isEmpty && !isSearching)
              _buildHonestEmptyDiscoveryState(theme)
            else if (devices.isEmpty && isSearching)
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 32),
                child: Center(
                  child: Column(
                    children: [
                      const CircularProgressIndicator(),
                      const SizedBox(height: 16),
                      Text(
                        'Scanning local subnet for DuetView beacons...',
                        style: theme.textTheme.bodyMedium?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
              )
            else
              ...devices.map(
                (dev) => PcDeviceCard(
                  device: dev,
                  isConnecting:
                      connState.isConnecting &&
                      connState.connectedDevice?.id == dev.id,
                  onConnect: () =>
                      _handleConnect(host: dev.address, port: dev.port, name: dev.name),
                ),
              ),
          ],
        );
      },
    );
  }

  Widget _buildHonestEmptyDiscoveryState(ThemeData theme) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          children: [
            Icon(
              Icons.search_off,
              size: 48,
              color: theme.colorScheme.onSurfaceVariant,
            ),
            const SizedBox(height: 12),
            Text(
              'No Windows PCs Detected',
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'The automatic UDP broadcast discovery server will be implemented in Phase 3 alongside the native Windows streaming engine. '
              'In the meantime, you can connect directly by providing your PC\'s local IP address.',
              textAlign: TextAlign.center,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
                height: 1.4,
              ),
            ),
            const SizedBox(height: 20),
            PrimaryButton(
              label: 'Connect Manually via IP',
              icon: Icons.edit_note,
              onPressed: () {
                setState(() {
                  _selectedTabIndex = 1;
                });
              },
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildManualView(ThemeData theme, ConnectionStateModel connState) {
    return Form(
      key: _formKey,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            'Manual Connection',
            style: theme.textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 6),
          Text(
            'Enter the local IPv4 address and port of your Windows PC.',
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
          const SizedBox(height: 16),

          // IP Address Field
          TextFormField(
            controller: _ipController,
            keyboardType: TextInputType.datetime,
            decoration: const InputDecoration(
              labelText: 'Windows PC IPv4 Address',
              hintText: 'e.g. 192.168.1.100',
              prefixIcon: Icon(Icons.lan_outlined),
            ),
            validator: (value) {
              if (value == null || value.trim().isEmpty) {
                return 'Please enter an IPv4 address';
              }
              if (!DefaultConnectionService.isValidIpv4(value)) {
                return 'Invalid IPv4 format (e.g. 192.168.1.100)';
              }
              return null;
            },
          ),
          const SizedBox(height: 14),

          // Port Field
          TextFormField(
            controller: _portController,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(
              labelText: 'Port',
              hintText: '52025',
              prefixIcon: Icon(Icons.tag),
            ),
            validator: (value) {
              if (value == null || value.trim().isEmpty) {
                return 'Please enter a port number';
              }
              final port = int.tryParse(value.trim());
              if (port == null || !DefaultConnectionService.isValidPort(port)) {
                return 'Port must be an integer between 1 and 65535';
              }
              return null;
            },
          ),
          const SizedBox(height: 20),

          // Connect Button
          PrimaryButton(
            label: 'Connect to PC',
            icon: Icons.link,
            isLoading: connState.isConnecting,
            onPressed: connState.isConnecting
                ? null
                : () {
                    if (_formKey.currentState?.validate() ?? false) {
                      _handleConnect();
                    }
                  },
          ),
          const SizedBox(height: 24),

          // Windows ipconfig helper tip
          Card(
            color: theme.colorScheme.surfaceContainerHighest.withValues(
              alpha: 0.3,
            ),
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Icon(
                        Icons.terminal,
                        size: 18,
                        color: theme.colorScheme.primary,
                      ),
                      const SizedBox(width: 8),
                      Text(
                        'How to find your PC\'s IP address:',
                        style: theme.textTheme.labelLarge?.copyWith(
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    '1. Press Windows Key + R, type "powershell" or "cmd", and press Enter.\n'
                    '2. Type "ipconfig" and look for "IPv4 Address" under your active Wi-Fi or Ethernet adapter.\n'
                    '3. Enter that address above.',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                      height: 1.45,
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
}
