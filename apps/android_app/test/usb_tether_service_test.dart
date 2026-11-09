import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/usb_tether_service.dart';

void main() {
  group('UsbTetherService Tests', () {
    test('UsbHostResolutionResult stores diagnostic fields accurately', () {
      const inactive = UsbHostResolutionResult(
        isTetheringActive: false,
        diagnosticMessage: 'USB Tethering is not detected.',
      );
      expect(inactive.isTetheringActive, isFalse);
      expect(inactive.isHostVerified, isFalse);
      expect(inactive.resolvedHostIp, isNull);
      expect(inactive.discoveryMethod, equals('none'));
      expect(inactive.diagnosticMessage, contains('not detected'));

      const verified = UsbHostResolutionResult(
        isTetheringActive: true,
        interfaceName: 'rndis0',
        androidIp: '192.168.42.129',
        subnetPrefix: '192.168.42',
        subnetBroadcast: '192.168.42.255',
        resolvedHostIp: '192.168.42.1',
        port: 52025,
        isHostVerified: true,
        discoveryMethod: 'probe_tcp_52025',
        diagnosticMessage: 'Verified DuetView PC on 192.168.42.1:52025 via TCP handshake probe.',
      );
      expect(verified.isTetheringActive, isTrue);
      expect(verified.interfaceName, equals('rndis0'));
      expect(verified.androidIp, equals('192.168.42.129'));
      expect(verified.resolvedHostIp, equals('192.168.42.1'));
      expect(verified.isHostVerified, isTrue);
      expect(verified.discoveryMethod, equals('probe_tcp_52025'));
    });

    test('isLocalAndroidIp identifies loopback and empty addresses', () async {
      expect(await UsbTetherService.isLocalAndroidIp(''), isTrue);
      expect(await UsbTetherService.isLocalAndroidIp('   '), isTrue);
      expect(await UsbTetherService.isLocalAndroidIp('127.0.0.1'), isTrue);
    });

    test('isLocalAndroidIp identifies actual active local host interface addresses', () async {
      final interfaces = await NetworkInterface.list(
        includeLoopback: true,
        type: InternetAddressType.IPv4,
      );
      for (final iface in interfaces) {
        for (final addr in iface.addresses) {
          final isLocal = await UsbTetherService.isLocalAndroidIp(addr.address);
          expect(isLocal, isTrue, reason: 'Expected ${addr.address} to be identified as local');
        }
      }
    });

    test('discoverUsbHost runs gracefully without crashing in test environment', () async {
      final result = await UsbTetherService.discoverUsbHost(
        port: 52025,
        probeTimeout: const Duration(milliseconds: 50),
      );
      expect(result, isNotNull);
      expect(result.port, equals(52025));
      expect(result.diagnosticMessage, isNotEmpty);
    });
  });
}

