import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

import '../models/saved_pc.dart';
import '../services/connection_service.dart';
import '../services/decoder_bridge.dart';
import '../services/qr_pairing_service.dart';
import '../services/saved_pc_storage.dart';
import '../services/settings_service.dart';

/// Screen allowing the user to scan or paste a Windows host pairing QR code.
class QrScannerScreen extends StatefulWidget {
  final SavedPcStorage storage;
  final ConnectionService? connectionService;
  final SettingsService? settingsService;
  final SecondScreenDecoderBridge? decoderBridge;

  const QrScannerScreen({
    super.key,
    required this.storage,
    this.connectionService,
    this.settingsService,
    this.decoderBridge,
  });

  @override
  State<QrScannerScreen> createState() => _QrScannerScreenState();
}

class _QrScannerScreenState extends State<QrScannerScreen> {
  final MobileScannerController _scannerController = MobileScannerController();
  bool _isProcessing = false;
  String? _statusMessage;
  bool _hasCameraError = false;
  MobileScannerException? _cameraException;

  @override
  void dispose() {
    _scannerController.dispose();
    super.dispose();
  }

  void _onDetect(BarcodeCapture capture) {
    if (_isProcessing) return;

    final List<Barcode> barcodes = capture.barcodes;
    for (final barcode in barcodes) {
      final rawValue = barcode.rawValue;
      if (rawValue != null && rawValue.isNotEmpty) {
        _processQrPayload(rawValue);
        break;
      }
    }
  }

  Future<void> _processQrPayload(String rawContent) async {
    setState(() {
      _isProcessing = true;
      _statusMessage = 'Validating QR code...';
    });

    final validation = QrPairingService.validate(rawContent);

    if (!validation.isValid || validation.payload == null) {
      setState(() {
        _isProcessing = false;
        _statusMessage = null;
      });

      _showErrorDialog(
        title: 'Invalid QR Code',
        message: validation.errorMessage ?? 'Could not parse pairing QR code.',
      );
      return;
    }

    final payload = validation.payload!;

    setState(() {
      _statusMessage = 'Pairing with ${payload.name} (${payload.id})...';
    });

    // Attempt network pairing handshake with candidate endpoints
    final handshake = await QrPairingService.pair(payload);

    if (!mounted) return;

    if (!handshake.isSuccess || handshake.savedPc == null) {
      setState(() {
        _isProcessing = false;
        _statusMessage = null;
      });

      _showErrorDialog(
        title: 'Pairing Failed',
        message: handshake.errorMessage ?? 'Could not complete pairing with host PC.',
      );
      return;
    }

    final savedPc = handshake.savedPc!;
    await widget.storage.savePc(savedPc);

    bool connectSucceeded = false;
    final connService = widget.connectionService;

    // Connect to streaming display port if connectionService is provided
    if (connService != null) {
      setState(() {
        _statusMessage = 'Connecting display stream to ${savedPc.name}...';
      });

      final targetHost = handshake.reachableEndpoint ?? savedPc.primaryAddress;
      connectSucceeded = await connService.connectToDevice(
        targetHost,
        savedPc.port,
        pcName: savedPc.name,
      );

      if (connectSucceeded) {
        await widget.storage.updatePcStatus(
          savedPc.id,
          PcOnlineStatus.streaming,
          reachableEndpoint: targetHost,
        );
      }
    }

    if (!mounted) return;

    setState(() {
      _isProcessing = false;
      _statusMessage = null;
    });

    // Show paired & connection confirmation dialog
    await showDialog(
      context: context,
      barrierDismissible: false,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1E293B),
        title: Row(
          children: [
            Icon(
              connectSucceeded ? Icons.check_circle : Icons.link,
              color: const Color(0xFF10B981),
            ),
            const SizedBox(width: 8),
            Text(
              connectSucceeded ? 'Paired & Connected' : 'PC Paired Successfully',
              style: const TextStyle(color: Colors.white, fontSize: 18),
            ),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              connectSucceeded
                  ? 'Successfully paired and connected to ${savedPc.name}.'
                  : 'Added ${savedPc.name} to your Saved PCs.',
              style: const TextStyle(color: Colors.white70),
            ),
            const SizedBox(height: 12),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: const Color(0xFF0F172A),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('PC ID: ${savedPc.id}', style: const TextStyle(color: Colors.white60, fontSize: 12)),
                  const SizedBox(height: 4),
                  Text(
                    'Streaming Address: ${handshake.reachableEndpoint ?? savedPc.primaryAddress}:${savedPc.port}',
                    style: const TextStyle(
                      color: Color(0xFF38BDF8),
                      fontSize: 13,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  if (connService != null && !connectSucceeded) ...[
                    const SizedBox(height: 6),
                    const Text(
                      'Host streaming service may be inactive. You can connect from the saved PCs list once started.',
                      style: TextStyle(color: Colors.white38, fontSize: 11),
                    ),
                  ],
                ],
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(ctx);
              Navigator.pop(context, savedPc);
            },
            child: const Text('Close', style: TextStyle(color: Colors.white70)),
          ),
          ElevatedButton.icon(
            style: ElevatedButton.styleFrom(
              backgroundColor: const Color(0xFF0284C7),
              foregroundColor: Colors.white,
            ),
            icon: Icon(connectSucceeded ? Icons.fullscreen : Icons.play_arrow, size: 18),
            label: Text(connectSucceeded ? 'Open Display' : 'Connect Now'),
            onPressed: () async {
              Navigator.pop(ctx);
              if (!connectSucceeded && widget.connectionService != null) {
                final targetHost = handshake.reachableEndpoint ?? savedPc.primaryAddress;
                await widget.connectionService!.connectToDevice(
                  targetHost,
                  savedPc.port,
                  pcName: savedPc.name,
                );
              }
              if (mounted) {
                Navigator.pop(context, savedPc);
              }
            },
          ),
        ],
      ),
    );
  }

  void _showErrorDialog({required String title, required String message}) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1E293B),
        title: Row(
          children: [
            const Icon(Icons.error_outline, color: Color(0xFFEF4444)),
            const SizedBox(width: 8),
            Text(title, style: const TextStyle(color: Colors.white, fontSize: 18)),
          ],
        ),
        content: Text(message, style: const TextStyle(color: Colors.white70, fontSize: 14)),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(ctx);
              setState(() {
                _isProcessing = false;
                _statusMessage = null;
              });
            },
            child: const Text('OK', style: TextStyle(color: Color(0xFF38BDF8))),
          ),
        ],
      ),
    ).then((_) {
      if (mounted) {
        setState(() {
          _isProcessing = false;
          _statusMessage = null;
        });
      }
    });
  }

  void _showManualEntryDialog() {
    final controller = TextEditingController();

    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1E293B),
        title: const Text('Paste Pairing QR Payload', style: TextStyle(color: Colors.white, fontSize: 18)),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Paste the pairing JSON string generated by the Windows host:',
              style: TextStyle(color: Colors.white70, fontSize: 13),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              maxLines: 4,
              style: const TextStyle(color: Colors.white, fontSize: 12, fontFamily: 'monospace'),
              decoration: InputDecoration(
                hintText: '{"v":1,"id":"...","name":"...","endpoints":[...],"token":"..."}',
                hintStyle: const TextStyle(color: Colors.white30, fontSize: 11),
                filled: true,
                fillColor: const Color(0xFF0F172A),
                border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel', style: TextStyle(color: Colors.white60)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF0284C7)),
            onPressed: () {
              final text = controller.text.trim();
              Navigator.pop(ctx);
              if (text.isNotEmpty) {
                _processQrPayload(text);
              }
            },
            child: const Text('Pair', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: const Text('Scan Pairing QR Code', style: TextStyle(fontSize: 18, color: Colors.white)),
        backgroundColor: Colors.black,
        iconTheme: const IconThemeData(color: Colors.white),
        actions: [
          IconButton(
            icon: const Icon(Icons.paste),
            tooltip: 'Paste pairing payload manually',
            onPressed: _showManualEntryDialog,
          ),
          IconButton(
            icon: ValueListenableBuilder<MobileScannerState>(
              valueListenable: _scannerController,
              builder: (context, state, _) {
                return Icon(
                  state.torchState == TorchState.on ? Icons.flash_on : Icons.flash_off,
                  color: Colors.white,
                );
              },
            ),
            tooltip: 'Toggle flashlight',
            onPressed: () => _scannerController.toggleTorch(),
          ),
        ],
      ),
      body: Stack(
        children: [
          // 1. Camera View or Fallback
          if (!_hasCameraError)
            MobileScanner(
              controller: _scannerController,
              onDetect: _onDetect,
              errorBuilder: (context, error) {
                _hasCameraError = true;
                _cameraException = error;
                return _buildCameraFallback(error);
              },
            )
          else
            _buildCameraFallback(_cameraException),

          // 2. Scanner Viewfinder Reticle Overlay
          if (!_hasCameraError) _buildViewfinderOverlay(),

          // 3. Processing overlay
          if (_isProcessing)
            Container(
              color: Colors.black.withValues(alpha: 0.7),
              child: Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const CircularProgressIndicator(color: Color(0xFF38BDF8)),
                    const SizedBox(height: 16),
                    Text(
                      _statusMessage ?? 'Processing...',
                      style: const TextStyle(color: Colors.white, fontSize: 16, fontWeight: FontWeight.w500),
                    ),
                  ],
                ),
              ),
            ),

          // 4. Bottom action hint
          Positioned(
            bottom: 24,
            left: 20,
            right: 20,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              decoration: BoxDecoration(
                color: const Color(0xFF1E293B).withValues(alpha: 0.9),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: Colors.white12),
              ),
              child: Row(
                children: [
                  const Icon(Icons.info_outline, color: Color(0xFF38BDF8), size: 20),
                  const SizedBox(width: 12),
                  const Expanded(
                    child: Text(
                      'Point camera at the QR code on the Windows host console or pairing HTML page.',
                      style: TextStyle(color: Colors.white70, fontSize: 12),
                    ),
                  ),
                  TextButton(
                    onPressed: _showManualEntryDialog,
                    child: const Text('Paste', style: TextStyle(color: Color(0xFF38BDF8), fontWeight: FontWeight.bold)),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildViewfinderOverlay() {
    return Center(
      child: Container(
        width: 260,
        height: 260,
        decoration: BoxDecoration(
          border: Border.all(color: const Color(0xFF38BDF8), width: 2.5),
          borderRadius: BorderRadius.circular(16),
        ),
      ),
    );
  }

  Widget _buildCameraFallback([MobileScannerException? error]) {
    final isPermissionDenied = error?.errorCode == MobileScannerErrorCode.permissionDenied;

    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              isPermissionDenied ? Icons.no_photography_outlined : Icons.camera_alt_outlined,
              color: Colors.white38,
              size: 64,
            ),
            const SizedBox(height: 16),
            Text(
              isPermissionDenied
                  ? 'Camera Permission Required'
                  : 'Camera Not Available',
              style: const TextStyle(color: Colors.white, fontSize: 16, fontWeight: FontWeight.bold),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 8),
            Text(
              isPermissionDenied
                  ? 'Camera permission was denied. Please grant camera access in device settings to scan QR codes, or paste the pairing payload manually.'
                  : 'Could not access the camera device. You can paste the pairing JSON string generated by the Windows host application.',
              style: const TextStyle(color: Colors.white60, fontSize: 13),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 20),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                OutlinedButton.icon(
                  style: OutlinedButton.styleFrom(
                    foregroundColor: const Color(0xFF38BDF8),
                    side: const BorderSide(color: Color(0xFF38BDF8)),
                    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
                  ),
                  icon: const Icon(Icons.refresh, size: 18),
                  label: const Text('Retry Camera'),
                  onPressed: () {
                    setState(() {
                      _hasCameraError = false;
                      _cameraException = null;
                    });
                    _scannerController.start();
                  },
                ),
                const SizedBox(width: 12),
                ElevatedButton.icon(
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFF0284C7),
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
                  ),
                  icon: const Icon(Icons.paste, size: 18),
                  label: const Text('Paste Payload'),
                  onPressed: _showManualEntryDialog,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
