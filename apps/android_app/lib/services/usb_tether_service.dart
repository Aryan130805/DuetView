import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

/// Diagnostic resolution result for USB tethered network discovery.
class UsbHostResolutionResult {
  final bool isTetheringActive;
  final String? interfaceName;
  final String? androidIp;
  final String? subnetPrefix;
  final String? subnetBroadcast;
  final String? resolvedHostIp;
  final int port;
  final bool isHostVerified;
  final String discoveryMethod;
  final String diagnosticMessage;

  const UsbHostResolutionResult({
    required this.isTetheringActive,
    this.interfaceName,
    this.androidIp,
    this.subnetPrefix,
    this.subnetBroadcast,
    this.resolvedHostIp,
    this.port = 52025,
    this.isHostVerified = false,
    this.discoveryMethod = 'none',
    required this.diagnosticMessage,
  });
}

/// Service dedicated to discovering and verifying the Windows DuetView PC host
/// over wired USB cable tethering.
class UsbTetherService {
  static const int defaultStreamingPort = 52025;
  static const int discoveryPort = 52026;

  /// Inspects active Android network interfaces and identifies USB tethering adapters (rndis, usb, ncm, eth).
  static Future<NetworkInterface?> getActiveUsbInterface() async {
    try {
      final interfaces = await NetworkInterface.list(
        includeLoopback: false,
        type: InternetAddressType.IPv4,
      );
      for (final iface in interfaces) {
        final name = iface.name.toLowerCase();
        if (name.contains('rndis') ||
            name.contains('usb') ||
            name.contains('ncm') ||
            name.contains('eth')) {
          for (final addr in iface.addresses) {
            if (addr.type == InternetAddressType.IPv4 &&
                !addr.isLoopback &&
                !addr.address.startsWith('127.') &&
                !addr.address.startsWith('169.254.')) {
              return iface;
            }
          }
        }
      }
    } catch (e) {
      debugPrint('UsbTetherService: Error querying network interfaces: $e');
    }
    return null;
  }

  /// Checks whether a given IP matches any local IP assigned to this Android device.
  static Future<bool> isLocalAndroidIp(String candidateIp) async {
    final clean = candidateIp.trim();
    if (clean.isEmpty || clean == '127.0.0.1') return true;
    try {
      final interfaces = await NetworkInterface.list(
        includeLoopback: true,
        type: InternetAddressType.IPv4,
      );
      for (final iface in interfaces) {
        for (final addr in iface.addresses) {
          if (addr.address == clean) return true;
        }
      }
    } catch (_) {}
    return false;
  }

  /// Automatically discovers and verifies the Windows host IP on the USB tethered link.
  static Future<UsbHostResolutionResult> discoverUsbHost({
    int port = defaultStreamingPort,
    Duration probeTimeout = const Duration(milliseconds: 250),
  }) async {
    final iface = await getActiveUsbInterface();
    if (iface == null) {
      return const UsbHostResolutionResult(
        isTetheringActive: false,
        diagnosticMessage: 'USB Tethering is not detected. Connect cable and enable USB Tethering in Android settings.',
      );
    }

    String? androidIp;
    for (final addr in iface.addresses) {
      if (addr.type == InternetAddressType.IPv4 && !addr.isLoopback) {
        androidIp = addr.address;
        break;
      }
    }

    if (androidIp == null) {
      return UsbHostResolutionResult(
        isTetheringActive: true,
        interfaceName: iface.name,
        diagnosticMessage: 'USB interface ${iface.name} detected, but no IPv4 address assigned.',
      );
    }

    final parts = androidIp.split('.');
    if (parts.length != 4) {
      return UsbHostResolutionResult(
        isTetheringActive: true,
        interfaceName: iface.name,
        androidIp: androidIp,
        diagnosticMessage: 'Invalid IPv4 address format on ${iface.name}.',
      );
    }

    final subnetPrefix = '${parts[0]}.${parts[1]}.${parts[2]}';
    final subnetBroadcast = '$subnetPrefix.255';

    // -----------------------------------------------------------------
    // Tier 1: Check Linux Kernel ARP Cache (/proc/net/arp)
    // -----------------------------------------------------------------
    final arpCandidates = await _getArpEntriesForInterface(iface.name, androidIp);
    for (final arpIp in arpCandidates) {
      if (await _testTcpPort(arpIp, port, probeTimeout)) {
        debugPrint('UsbTetherService: Verified host at $arpIp:$port via ARP entry');
        return UsbHostResolutionResult(
          isTetheringActive: true,
          interfaceName: iface.name,
          androidIp: androidIp,
          subnetPrefix: subnetPrefix,
          subnetBroadcast: subnetBroadcast,
          resolvedHostIp: arpIp,
          port: port,
          isHostVerified: true,
          discoveryMethod: 'arp',
          diagnosticMessage: 'Verified Windows host at $arpIp via USB ARP cache.',
        );
      }
    }

    // -----------------------------------------------------------------
    // Tier 2: Directed Subnet UDP Discovery Broadcast
    // -----------------------------------------------------------------
    final udpHost = await _probeSubnetUdp(subnetBroadcast, port);
    if (udpHost != null && udpHost != androidIp) {
      if (await _testTcpPort(udpHost, port, probeTimeout)) {
        debugPrint('UsbTetherService: Verified host at $udpHost:$port via UDP beacon');
        return UsbHostResolutionResult(
          isTetheringActive: true,
          interfaceName: iface.name,
          androidIp: androidIp,
          subnetPrefix: subnetPrefix,
          subnetBroadcast: subnetBroadcast,
          resolvedHostIp: udpHost,
          port: port,
          isHostVerified: true,
          discoveryMethod: 'udp_beacon',
          diagnosticMessage: 'Discovered Windows host at $udpHost via USB directed broadcast.',
        );
      }
    }

    // -----------------------------------------------------------------
    // Tier 3: Subnet Candidate TCP Port Probing
    // -----------------------------------------------------------------
    // If Android is .129 (default AOSP tethering gateway), common Windows client IPs are .1, .2, .100, .254
    // If Android is .1, common Windows client IPs are .2, .129, .100
    final candidateHosts = <String>[];
    if (parts[3] == '129') {
      candidateHosts.addAll(['$subnetPrefix.1', '$subnetPrefix.2', '$subnetPrefix.100', '$subnetPrefix.254']);
    } else if (parts[3] == '1') {
      candidateHosts.addAll(['$subnetPrefix.2', '$subnetPrefix.129', '$subnetPrefix.100']);
    } else {
      candidateHosts.addAll(['$subnetPrefix.1', '$subnetPrefix.2', '$subnetPrefix.129']);
    }

    for (final cand in candidateHosts) {
      if (cand != androidIp && await _testTcpPort(cand, port, probeTimeout)) {
        debugPrint('UsbTetherService: Verified host at $cand:$port via subnet probe');
        return UsbHostResolutionResult(
          isTetheringActive: true,
          interfaceName: iface.name,
          androidIp: androidIp,
          subnetPrefix: subnetPrefix,
          subnetBroadcast: subnetBroadcast,
          resolvedHostIp: cand,
          port: port,
          isHostVerified: true,
          discoveryMethod: 'subnet_probe',
          diagnosticMessage: 'Verified Windows host at $cand via direct TCP probe.',
        );
      }
    }

    // Fallback: If host is not currently answering port 52025, pick the best candidate
    final fallbackHost = arpCandidates.isNotEmpty
        ? arpCandidates.first
        : (candidateHosts.isNotEmpty ? candidateHosts.first : '$subnetPrefix.1');

    return UsbHostResolutionResult(
      isTetheringActive: true,
      interfaceName: iface.name,
      androidIp: androidIp,
      subnetPrefix: subnetPrefix,
      subnetBroadcast: subnetBroadcast,
      resolvedHostIp: fallbackHost,
      port: port,
      isHostVerified: false,
      discoveryMethod: 'candidate_fallback',
      diagnosticMessage: 'USB tethering active on ${iface.name} ($androidIp). Host candidate: $fallbackHost (port $port unverified).',
    );
  }

  static Future<List<String>> _getArpEntriesForInterface(String ifaceName, String localIp) async {
    final list = <String>[];
    try {
      final file = File('/proc/net/arp');
      if (await file.exists()) {
        final lines = await file.readAsLines();
        for (final line in lines.skip(1)) {
          final tokens = line.trim().split(RegExp(r'\s+'));
          if (tokens.length >= 6) {
            final ip = tokens[0];
            final dev = tokens[5];
            if (dev == ifaceName && ip != localIp && ip.split('.').length == 4) {
              list.add(ip);
            }
          }
        }
      }
    } catch (_) {}
    return list;
  }

  static Future<String?> _probeSubnetUdp(String broadcastAddress, int port) async {
    RawDatagramSocket? socket;
    try {
      socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
      socket.broadcastEnabled = true;

      final completer = Completer<String?>();
      final timer = Timer(const Duration(milliseconds: 400), () {
        if (!completer.isCompleted) completer.complete(null);
      });

      socket.listen((event) {
        if (event == RawSocketEvent.read) {
          final dg = socket?.receive();
          if (dg != null) {
            final text = utf8.decode(dg.data).trim();
            if (text.startsWith('SECONDSCREEN_BEACON:')) {
              timer.cancel();
              if (!completer.isCompleted) {
                completer.complete(dg.address.address);
              }
            }
          }
        }
      });

      final probeMsg = utf8.encode('SECONDSCREEN_DISCOVER');
      socket.send(probeMsg, InternetAddress(broadcastAddress), discoveryPort);

      final result = await completer.future;
      return result;
    } catch (_) {
      return null;
    } finally {
      socket?.close();
    }
  }

  static Future<bool> _testTcpPort(String host, int port, Duration timeout) async {
    try {
      final socket = await Socket.connect(host, port, timeout: timeout);
      socket.destroy();
      return true;
    } catch (_) {
      return false;
    }
  }
}

