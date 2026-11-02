import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';
import 'package:second_screen_android/services/qr_pairing_service.dart';

class FakePairSocket extends Stream<Uint8List> implements Socket {
  final StreamController<Uint8List> _controller = StreamController<Uint8List>();
  final List<Uint8List> sentBytes = [];

  void emitResponse(int statusCode, {String name = 'Test PC', String id = 'pc-123'}) {
    final respJson = '{"status":$statusCode,"name":"$name","id":"$id"}';
    final payloadBytes = Uint8List.fromList(utf8.encode(respJson));
    final header = ByteData(24);
    header.setUint8(0, 0x53);
    header.setUint8(1, 0x53);
    header.setUint8(2, 0x30);
    header.setUint8(3, 0x32);
    header.setUint16(4, 2, Endian.big);
    header.setUint16(6, Ss02PacketType.pairResponse.value, Endian.big);
    header.setUint32(8, 1, Endian.big);
    header.setUint32(12, payloadBytes.length, Endian.big);
    header.setUint64(16, 1000000, Endian.big);

    final b = BytesBuilder();
    b.add(header.buffer.asUint8List());
    b.add(payloadBytes);
    _controller.add(b.toBytes());
  }

  @override
  void add(List<int> data) {
    sentBytes.add(Uint8List.fromList(data));
  }

  @override
  Future flush() async {}

  @override
  StreamSubscription<Uint8List> listen(
    void Function(Uint8List event)? onData, {
    Function? onError,
    void Function()? onDone,
    bool? cancelOnError,
  }) {
    return _controller.stream.listen(onData, onError: onError, onDone: onDone, cancelOnError: cancelOnError);
  }

  @override
  void destroy() {
    _controller.close();
  }

  @override
  Future close() async {
    _controller.close();
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}

void main() {
  group('QrPairingService Payload Validation Tests', () {
    test('Validates canonical QR payload successfully', () {
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;
      final validJson = jsonEncode({
        'v': 1,
        'id': 'pc-abc12345-6789',
        'name': 'Workstation-Pro',
        'endpoints': ['192.168.1.150', '10.0.0.5'],
        'port': 52025,
        'pair_port': 52027,
        'token': 'a1b2c3d4e5f6789012345678abcdef01',
        'exp': nowSec + 300, // Valid for 5 minutes
      });

      final result = QrPairingService.validate(validJson);
      expect(result.isValid, isTrue);
      expect(result.errorMessage, isNull);
      expect(result.payload, isNotNull);

      final p = result.payload!;
      expect(p.version, equals(1));
      expect(p.id, equals('pc-abc12345-6789'));
      expect(p.name, equals('Workstation-Pro'));
      expect(p.endpoints, containsAll(['192.168.1.150', '10.0.0.5']));
      expect(p.port, equals(52025));
      expect(p.pairPort, equals(52027));
      expect(p.token, equals('a1b2c3d4e5f6789012345678abcdef01'));
      expect(p.isExpired, isFalse);
    });

    test('Rejects expired pairing token with actionable error', () {
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;
      final expiredJson = jsonEncode({
        'v': 1,
        'id': 'pc-expired-123',
        'name': 'Old-PC',
        'endpoints': ['192.168.1.50'],
        'port': 52025,
        'token': 'token12345',
        'exp': nowSec - 10, // Expired 10 seconds ago
      });

      final result = QrPairingService.validate(expiredJson);
      expect(result.isValid, isFalse);
      expect(result.errorMessage, contains('expired'));
      expect(result.errorMessage, contains('5 minutes'));
    });

    test('Rejects unsupported protocol version', () {
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;
      final futureVersionJson = jsonEncode({
        'v': 0,
        'id': 'pc-123',
        'name': 'PC',
        'endpoints': ['192.168.1.50'],
        'port': 52025,
        'token': 'token123',
        'exp': nowSec + 300,
      });

      final result = QrPairingService.validate(futureVersionJson);
      expect(result.isValid, isFalse);
      expect(result.errorMessage, contains('version'));
    });

    test('Rejects malformed JSON and non-JSON input', () {
      final malformed = 'NOT_JSON_AT_ALL';
      final result1 = QrPairingService.validate(malformed);
      expect(result1.isValid, isFalse);

      final brokenJson = '{"v": 1, "id": "pc-1",';
      final result2 = QrPairingService.validate(brokenJson);
      expect(result2.isValid, isFalse);
    });

    test('Rejects payload with missing mandatory fields', () {
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;

      // Missing token
      final missingToken = jsonEncode({
        'v': 1,
        'id': 'pc-1',
        'name': 'PC',
        'endpoints': ['192.168.1.1'],
        'port': 52025,
        'exp': nowSec + 300,
      });
      expect(QrPairingService.validate(missingToken).isValid, isFalse);

      // Missing endpoints
      final missingEndpoints = jsonEncode({
        'v': 1,
        'id': 'pc-1',
        'name': 'PC',
        'endpoints': [],
        'port': 52025,
        'token': 'tok',
        'exp': nowSec + 300,
      });
      expect(QrPairingService.validate(missingEndpoints).isValid, isFalse);

      // Illegal port
      final illegalPort = jsonEncode({
        'v': 1,
        'id': 'pc-1',
        'name': 'PC',
        'endpoints': ['192.168.1.1'],
        'port': 70000, // Out of range
        'token': 'tok',
        'exp': nowSec + 300,
      });
      expect(QrPairingService.validate(illegalPort).isValid, isFalse);
    });
  });

  group('QrPairingService Network Handshake Tests', () {
    test('Successful handshake creates verified SavedPc with reachable endpoint', () async {
      final fakeSocket = FakePairSocket();
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;

      final payload = QrPairingPayload(
        version: 1,
        id: 'pc-verified-1',
        name: 'Main Desktop',
        endpoints: ['192.168.1.88'],
        port: 52025,
        pairPort: 52027,
        token: 'valid-secret-token',
        expirySec: nowSec + 300,
      );

      final pairFuture = QrPairingService.pair(
        payload,
        socketConnector: (host, port, {timeout}) async {
          expect(host, equals('192.168.1.88'));
          expect(port, equals(52027));
          return fakeSocket;
        },
      );

      // Give event loop time to write packet, then host emits success response
      await Future.delayed(const Duration(milliseconds: 10));
      fakeSocket.emitResponse(0, name: 'Main Desktop', id: 'pc-verified-1');

      final result = await pairFuture;
      expect(result.isSuccess, isTrue);
      expect(result.reachableEndpoint, equals('192.168.1.88'));
      expect(result.savedPc, isNotNull);

      final pc = result.savedPc!;
      expect(pc.id, equals('pc-verified-1'));
      expect(pc.name, equals('Main Desktop'));
      expect(pc.primaryAddress, equals('192.168.1.88'));
      expect(pc.port, equals(52025));
    });

    test('Candidate port fallback tries streaming port if pairPort is unreachable', () async {
      final fakeSocket = FakePairSocket();
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;
      final triedPorts = <int>[];

      final payload = QrPairingPayload(
        version: 1,
        id: 'pc-fallback-port',
        name: 'Standalone Engine',
        endpoints: ['192.168.1.99'],
        port: 52025,
        pairPort: 52027,
        token: 'token-fallback',
        expirySec: nowSec + 300,
      );

      final pairFuture = QrPairingService.pair(
        payload,
        socketConnector: (host, port, {timeout}) async {
          triedPorts.add(port);
          if (port == 52027) {
            throw const SocketException('Connection refused on 52027');
          }
          return fakeSocket;
        },
      );

      await Future.delayed(const Duration(milliseconds: 10));
      fakeSocket.emitResponse(0, name: 'Standalone Engine', id: 'pc-fallback-port');

      final result = await pairFuture;
      expect(result.isSuccess, isTrue);
      expect(triedPorts, equals([52027, 52025]));
      expect(result.savedPc?.name, equals('Standalone Engine'));
    });

    test('Rejects handshake when token is expired on the host', () async {
      final fakeSocket = FakePairSocket();
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;

      final payload = QrPairingPayload(
        version: 1,
        id: 'pc-expired-host',
        name: 'PC',
        endpoints: ['192.168.1.50'],
        port: 52025,
        pairPort: 52027,
        token: 'token',
        expirySec: nowSec + 300,
      );

      final pairFuture = QrPairingService.pair(
        payload,
        socketConnector: (host, port, {timeout}) async => fakeSocket,
      );

      await Future.delayed(const Duration(milliseconds: 10));
      fakeSocket.emitResponse(2); // statusCode 2 = Expired

      final result = await pairFuture;
      expect(result.isSuccess, isFalse);
      expect(result.errorMessage, contains('expired'));
    });

    test('Rejects handshake when token is invalid or refused by host', () async {
      final fakeSocket = FakePairSocket();
      final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;

      final payload = QrPairingPayload(
        version: 1,
        id: 'pc-invalid-token',
        name: 'PC',
        endpoints: ['192.168.1.50'],
        port: 52025,
        pairPort: 52027,
        token: 'bad-token',
        expirySec: nowSec + 300,
      );

      final pairFuture = QrPairingService.pair(
        payload,
        socketConnector: (host, port, {timeout}) async => fakeSocket,
      );

      await Future.delayed(const Duration(milliseconds: 10));
      fakeSocket.emitResponse(1); // statusCode 1 = Invalid

      final result = await pairFuture;
      expect(result.isSuccess, isFalse);
      expect(result.errorMessage, contains('refused authentication token'));
    });
  });
}
