import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/services/connection_service.dart';

class FakeSocket extends Stream<Uint8List> implements Socket {
  final StreamController<Uint8List> _controller = StreamController<Uint8List>();
  bool wasDestroyed = false;
  bool wasClosed = false;

  @override
  StreamSubscription<Uint8List> listen(
    void Function(Uint8List event)? onData, {
    Function? onError,
    void Function()? onDone,
    bool? cancelOnError,
  }) {
    return _controller.stream.listen(
      onData,
      onError: onError,
      onDone: onDone,
      cancelOnError: cancelOnError,
    );
  }

  @override
  void destroy() {
    wasDestroyed = true;
  }

  @override
  Future close() async {
    wasClosed = true;
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}

void main() {
  group('ConnectionService Validation Tests', () {
    test('validates correct IPv4 addresses', () {
      expect(DefaultConnectionService.isValidIpv4('192.168.1.1'), isTrue);
      expect(DefaultConnectionService.isValidIpv4('10.0.0.1'), isTrue);
      expect(DefaultConnectionService.isValidIpv4('172.16.0.100'), isTrue);
      expect(DefaultConnectionService.isValidIpv4('127.0.0.1'), isTrue);
      expect(DefaultConnectionService.isValidIpv4('  192.168.1.50  '), isTrue);
    });

    test('rejects invalid IPv4 addresses', () {
      expect(DefaultConnectionService.isValidIpv4(''), isFalse);
      expect(DefaultConnectionService.isValidIpv4('192.168.1'), isFalse);
      expect(DefaultConnectionService.isValidIpv4('192.168.1.256'), isFalse);
      expect(DefaultConnectionService.isValidIpv4('192.168.1.1.1'), isFalse);
      expect(DefaultConnectionService.isValidIpv4('abc.def.ghi.jkl'), isFalse);
      expect(DefaultConnectionService.isValidIpv4('192.168.1.-1'), isFalse);
    });

    test('validates port range', () {
      expect(DefaultConnectionService.isValidPort(1), isTrue);
      expect(DefaultConnectionService.isValidPort(52025), isTrue);
      expect(DefaultConnectionService.isValidPort(65535), isTrue);
      expect(DefaultConnectionService.isValidPort(0), isFalse);
      expect(DefaultConnectionService.isValidPort(65536), isFalse);
      expect(DefaultConnectionService.isValidPort(-5), isFalse);
    });
  });

  group('ConnectionService Lifecycle & State Transition Tests', () {
    test(
      'transitions through connecting to connected on successful socket probe',
      () async {
        final fakeSocket = FakeSocket();
        final service = DefaultConnectionService(
          socketConnector: (host, port, {timeout}) async => fakeSocket,
        );

        final stateHistory = <ConnectionStatus>[];
        service.addListener(() {
          stateHistory.add(service.state.status);
        });

        expect(service.state.status, ConnectionStatus.disconnected);

        final success = await service.connectToDevice('192.168.1.100', 52025);

        expect(success, isTrue);
        expect(service.state.status, ConnectionStatus.connected);
        expect(service.state.connectedDevice?.address, '192.168.1.100');
        expect(service.state.connectedDevice?.port, 52025);
        expect(service.state.pingMs, isNotNull);
        expect(stateHistory, [
          ConnectionStatus.connecting,
          ConnectionStatus.connected,
        ]);

        // Disconnect
        await service.disconnect();
        expect(service.state.status, ConnectionStatus.disconnected);
        expect(service.state.connectedDevice, isNull);
      },
    );

    test(
      'transitions to failed state when socket throws SocketException',
      () async {
        final service = DefaultConnectionService(
          socketConnector: (host, port, {timeout}) async {
            throw const SocketException('Connection refused');
          },
        );

        final success = await service.connectToDevice('192.168.1.100', 52025);

        expect(success, isFalse);
        expect(service.state.status, ConnectionStatus.failed);
        expect(service.state.errorMessage, contains('Could not connect'));
      },
    );

    test('transitions to failed state when socket times out', () async {
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async {
          throw TimeoutException('Timed out');
        },
      );

      final success = await service.connectToDevice('192.168.1.100', 52025);

      expect(success, isFalse);
      expect(service.state.status, ConnectionStatus.failed);
      expect(service.state.errorMessage, contains('timed out'));
    });

    test(
      'rejects connection if IPv4 is invalid without attempting socket',
      () async {
        var socketAttempted = false;
        final service = DefaultConnectionService(
          socketConnector: (host, port, {timeout}) async {
            socketAttempted = true;
            return FakeSocket();
          },
        );

        final success = await service.connectToDevice('invalid.ip', 52025);

        expect(success, isFalse);
        expect(socketAttempted, isFalse);
        expect(service.state.status, ConnectionStatus.failed);
        expect(service.state.errorMessage, contains('Invalid IPv4 address'));
      },
    );
  });
}
