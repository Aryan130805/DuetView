import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../models/pc_device.dart';

/// Service responsible for local network UDP discovery of SecondScreen host PCs.
class DiscoveryService extends ChangeNotifier {
  static const int discoveryPort = 52026;
  static const String probeMessage = 'SECONDSCREEN_DISCOVER';

  bool _isSearching = false;
  final List<PcDevice> _discoveredDevices = [];
  Timer? _searchTimer;
  Timer? _probeTimer;
  RawDatagramSocket? _socket;

  bool get isSearching => _isSearching;
  List<PcDevice> get discoveredDevices => List.unmodifiable(_discoveredDevices);

  /// Initiates local network discovery scanning window using UDP broadcast.
  Future<void> startDiscovery({
    Duration timeout = const Duration(seconds: 5),
  }) async {
    if (_isSearching) return;

    _isSearching = true;
    _discoveredDevices.clear();
    notifyListeners();

    try {
      _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
      _socket?.broadcastEnabled = true;

      _socket?.listen((event) {
        if (event == RawSocketEvent.read) {
          final datagram = _socket?.receive();
          if (datagram != null) {
            _handleDatagram(datagram);
          }
        }
      });

      // Send initial probe and repeat every 1 second
      _sendProbe();
      _probeTimer?.cancel();
      _probeTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        if (_isSearching) {
          _sendProbe();
        }
      });
    } catch (e) {
      debugPrint('DiscoveryService: Failed to bind UDP socket: $e');
    }

    _searchTimer?.cancel();
    _searchTimer = Timer(timeout, () {
      stopDiscovery();
    });
  }

  void _sendProbe() {
    try {
      final probeData = utf8.encode(probeMessage);
      _socket?.send(
        probeData,
        InternetAddress('255.255.255.255'),
        discoveryPort,
      );
    } catch (e) {
      debugPrint('DiscoveryService: Error sending probe: $e');
    }
  }

  void _handleDatagram(Datagram datagram) {
    try {
      final text = utf8.decode(datagram.data).trim();
      if (text.startsWith('SECONDSCREEN_BEACON:')) {
        final jsonStr = text.substring('SECONDSCREEN_BEACON:'.length);
        final map = jsonDecode(jsonStr) as Map<String, dynamic>;
        _registerHost(datagram.address.address, map);
      } else if (text.startsWith('{') && text.endsWith('}')) {
        final map = jsonDecode(text) as Map<String, dynamic>;
        _registerHost(datagram.address.address, map);
      }
    } catch (e) {
      debugPrint('DiscoveryService: Error parsing datagram: $e');
    }
  }

  void _registerHost(String senderAddress, Map<String, dynamic> map) {
    final name = map['name'] as String? ?? 'Windows PC';
    final port = (map['port'] as num?)?.toInt() ?? 52025;
    final osName = map['osName'] as String? ?? 'Windows';
    final hostAddr = map['address'] as String? ?? senderAddress;

    final device = PcDevice(
      id: '$hostAddr:$port',
      name: name,
      address: hostAddr,
      port: port,
      isReachable: true,
      lastSeen: DateTime.now(),
      osName: osName,
    );

    addDevice(device);
  }

  /// Manually stops the in-progress discovery.
  void stopDiscovery() {
    _searchTimer?.cancel();
    _searchTimer = null;
    _probeTimer?.cancel();
    _probeTimer = null;
    try {
      _socket?.close();
    } catch (_) {}
    _socket = null;
    _isSearching = false;
    notifyListeners();
  }

  /// Adds a manually discovered or beaconed device.
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
    stopDiscovery();
    super.dispose();
  }
}
