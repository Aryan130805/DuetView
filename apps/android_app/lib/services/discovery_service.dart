import 'dart:async';

import 'package:flutter/foundation.dart';

import '../models/pc_device.dart';

/// Service responsible for local network discovery of SecondScreen host PCs.
///
/// NOTE: In this Phase 2 MVP, no Windows broadcast responder exists yet.
/// This service provides the discovery interface and state machine,
/// cleanly communicating the scanning state and reporting the honest empty state
/// when no hosts respond on the local subnet.
class DiscoveryService extends ChangeNotifier {
  bool _isSearching = false;
  final List<PcDevice> _discoveredDevices = [];
  Timer? _searchTimer;

  bool get isSearching => _isSearching;
  List<PcDevice> get discoveredDevices => List.unmodifiable(_discoveredDevices);

  /// Initiates local network discovery scanning window.
  Future<void> startDiscovery({
    Duration timeout = const Duration(seconds: 3),
  }) async {
    if (_isSearching) return;

    _isSearching = true;
    _discoveredDevices.clear();
    notifyListeners();

    _searchTimer?.cancel();
    _searchTimer = Timer(timeout, () {
      _isSearching = false;
      notifyListeners();
    });
  }

  /// Manually stops the in-progress discovery.
  void stopDiscovery() {
    if (!_isSearching) return;
    _searchTimer?.cancel();
    _isSearching = false;
    notifyListeners();
  }

  /// Adds a manually discovered or beaconed device (used for testing and future mDNS/UDP listeners).
  void addDevice(PcDevice device) {
    final index = _discoveredDevices.indexWhere((d) => d.id == device.id);
    if (index >= 0) {
      _discoveredDevices[index] = device;
    } else {
      _discoveredDevices.add(device);
    }
    notifyListeners();
  }

  /// Clears the discovered device list.
  void clear() {
    _discoveredDevices.clear();
    notifyListeners();
  }

  @override
  void dispose() {
    _searchTimer?.cancel();
    super.dispose();
  }
}
