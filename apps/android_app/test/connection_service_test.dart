import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/models/connection_state.dart';
import 'package:second_screen_android/services/connection_service.dart';
import 'package:second_screen_android/services/decoder_bridge.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';

class FakeSocket extends Stream<Uint8List> implements Socket {
  final StreamController<Uint8List> _controller = StreamController<Uint8List>();
  final List<Uint8List> sentBytes = [];
  bool wasDestroyed = false;
  bool wasClosed = false;

  void emitChunk(Uint8List chunk) {
    _controller.add(chunk);
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
    _controller.close();
  }

  @override
  Future close() async {
    wasClosed = true;
    _controller.close();
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}

Uint8List buildSs01FramePacket(Uint8List payload) {
  final header = ByteData(20);
  header.setUint8(0, 0x53);
  header.setUint8(1, 0x53);
  header.setUint8(2, 0x30);
  header.setUint8(3, 0x31);
  header.setUint8(4, 0x01);
  header.setUint32(8, payload.length, Endian.big);
  header.setUint64(12, 1700000000000, Endian.big);

  final builder = BytesBuilder();
  builder.add(header.buffer.asUint8List());
  builder.add(payload);
  return builder.toBytes();
}

Uint8List buildSs02Packet(Ss02PacketType type, Uint8List payload, {int sequenceNumber = 1, int timestampUs = 1000000}) {
  final header = ByteData(24);
  header.setUint8(0, 0x53);
  header.setUint8(1, 0x53);
  header.setUint8(2, 0x30);
  header.setUint8(3, 0x32);
  header.setUint16(4, 2, Endian.big);
  header.setUint16(6, type.value, Endian.big);
  header.setUint32(8, sequenceNumber, Endian.big);
  header.setUint32(12, payload.length, Endian.big);
  header.setUint64(16, timestampUs, Endian.big);

  final builder = BytesBuilder();
  builder.add(header.buffer.asUint8List());
  builder.add(payload);
  return builder.toBytes();
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('com.secondscreen/decoder_control');
  final List<MethodCall> decoderCallLog = <MethodCall>[];
  bool mockIsSupported = true;

  setUp(() {
    decoderCallLog.clear();
    mockIsSupported = true;

    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
      decoderCallLog.add(methodCall);
      switch (methodCall.method) {
        case 'getDecoderCapabilities':
          return {
            'codecName': 'c2.qti.avc.decoder',
            'isHardwareAccelerated': true,
            'maxWidth': 3840,
            'maxHeight': 2160,
            'isSupported': mockIsSupported,
          };
        case 'initializeDecoder':
          return true;
        case 'feedConfigPacket':
          return true;
        case 'feedAccessUnit':
          return true;
        case 'resetDecoder':
          return true;
        default:
          return null;
      }
    });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

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

  group('ConnectionService Lifecycle & SS01 Fallback Tests', () {
    test('transitions through connecting to connected, then streaming on incoming SS01 frame', () async {
      final fakeSocket = FakeSocket();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async => fakeSocket,
      );

      expect(service.state.status, ConnectionStatus.disconnected);

      final success = await service.connectToDevice('192.168.1.100', 52025);

      expect(success, isTrue);
      expect(service.state.status, ConnectionStatus.connected);
      expect(service.state.connectedDevice?.address, '192.168.1.100');
      expect(service.latestFrame.value, isNull);

      // Host sends SS01 frame packet
      final fakeJpeg = Uint8List.fromList([0xFF, 0xD8, 0x11, 0x22, 0xFF, 0xD9]);
      fakeSocket.emitChunk(buildSs01FramePacket(fakeJpeg));

      await Future.delayed(Duration.zero);

      expect(service.state.status, ConnectionStatus.streaming);
      expect(service.state.activeProtocol, StreamingProtocol.ss01Jpeg);
      expect(service.state.totalFramesReceived, 1);
      expect(service.latestFrame.value, fakeJpeg);

      await service.disconnect();
      expect(service.state.status, ConnectionStatus.disconnected);
      expect(service.state.connectedDevice, isNull);
      expect(service.latestFrame.value, isNull);
    });

    test('selects SS01 JPEG fallback when H.264 decoder is unsupported', () async {
      mockIsSupported = false; // Simulate unsupported hardware decoder

      final fakeSocket = FakeSocket();
      final bridge = SecondScreenDecoderBridge();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async => fakeSocket,
        decoderBridge: bridge,
      );

      final success = await service.connectToDevice('192.168.1.100', 52025);

      expect(success, isTrue);
      expect(service.state.activeProtocol, equals(StreamingProtocol.ss01Jpeg));
      expect(service.state.fallbackReason, contains('H.264 decoder not supported'));

      await service.disconnect();
      service.dispose();
      bridge.dispose();
    });

    test('socket connection failure does not alter decoder compatibility state', () async {
      final bridge = SecondScreenDecoderBridge();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async {
          throw const SocketException('Connection refused');
        },
        decoderBridge: bridge,
      );

      final success = await service.connectToDevice('192.168.1.100', 52025);

      expect(success, isFalse);
      expect(service.state.status, ConnectionStatus.failed);
      expect(service.state.fallbackReason, isNull); // Must not falsely set fallback on network fail

      await service.disconnect();
      service.dispose();
      bridge.dispose();
    });
  });

  group('ConnectionService SS02 H.264 Routing & Keyframe Tests', () {
    test('routes CONFIG, KEYFRAME, and DELTA packets to decoder bridge in sequence', () async {
      final fakeSocket = FakeSocket();
      final bridge = SecondScreenDecoderBridge();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async => fakeSocket,
        decoderBridge: bridge,
      );

      await service.connectToDevice('192.168.1.100', 52025);
      expect(service.state.status, ConnectionStatus.connected);

      // 1. Host sends CONFIG packet (SPS/PPS)
      final spsPps = Uint8List.fromList([0, 0, 0, 1, 0x67, 42, 0, 0, 0, 1, 0x68, 20]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.config, spsPps, sequenceNumber: 1));

      await Future.delayed(Duration.zero);
      expect(service.state.hasCodecConfig, isTrue);
      expect(service.state.configPacketsReceived, 1);
      expect(decoderCallLog.any((c) => c.method == 'feedConfigPacket'), isTrue);

      // 2. Host sends IDR KEYFRAME packet
      final idrData = Uint8List.fromList([0, 0, 0, 1, 0x65, 88, 99]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.keyframe, idrData, sequenceNumber: 2, timestampUs: 500000));

      await Future.delayed(Duration.zero);
      expect(service.state.isStreaming, isTrue);
      expect(service.state.hasReceivedFirstKeyframe, isTrue);
      expect(service.state.keyframePacketsReceived, 1);

      final keyframeCall = decoderCallLog.firstWhere((c) => c.method == 'feedAccessUnit' && c.arguments['isKeyframe'] == true);
      expect(keyframeCall.arguments['timestampUs'], equals(500000));
      expect(keyframeCall.arguments['data'], equals(idrData));

      // 3. Host sends DELTA frame packet
      final deltaData = Uint8List.fromList([0, 0, 0, 1, 0x41, 10, 20]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.delta, deltaData, sequenceNumber: 3, timestampUs: 533333));

      await Future.delayed(Duration.zero);
      expect(service.state.deltaPacketsReceived, 1);
      final deltaCall = decoderCallLog.firstWhere((c) => c.method == 'feedAccessUnit' && c.arguments['isKeyframe'] == false);
      expect(deltaCall.arguments['timestampUs'], equals(533333));
      expect(deltaCall.arguments['data'], equals(deltaData));

      await service.disconnect();
      service.dispose();
      bridge.dispose();
    });

    test('discards delta frames arriving before keyframe and requests keyframe', () async {
      final fakeSocket = FakeSocket();
      final bridge = SecondScreenDecoderBridge();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async => fakeSocket,
        decoderBridge: bridge,
      );

      await service.connectToDevice('192.168.1.100', 52025);

      // Config arrives
      final spsPps = Uint8List.fromList([0, 0, 0, 1, 0x67, 1, 0, 0, 0, 1, 0x68, 2]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.config, spsPps, sequenceNumber: 1));
      await Future.delayed(Duration.zero);

      // Delta frame arrives BEFORE any Keyframe
      final deltaData = Uint8List.fromList([0, 0, 0, 1, 0x41, 99]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.delta, deltaData, sequenceNumber: 2));
      await Future.delayed(Duration.zero);

      // Delta frame should NOT be submitted to decoder
      expect(decoderCallLog.any((c) => c.method == 'feedAccessUnit'), isFalse);
      // Upstream keyframe request must be sent
      expect(fakeSocket.sentBytes.isNotEmpty, isTrue);
      expect(fakeSocket.sentBytes.first.length, equals(24));

      final view = ByteData.sublistView(fakeSocket.sentBytes.first);
      expect(view.getUint16(6, Endian.big), equals(Ss02PacketType.keyframeRequest.value));

      await service.disconnect();
      service.dispose();
      bridge.dispose();
    });

    test('sends upstream PACKET_TYPE_KEYFRAME_REQ when decoder bridge requests keyframe', () async {
      final fakeSocket = FakeSocket();
      final bridge = SecondScreenDecoderBridge();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async => fakeSocket,
        decoderBridge: bridge,
      );

      await service.connectToDevice('192.168.1.100', 52025);

      // Simulate native platform emitting onKeyframeRequested
      final keyframeMethodCall = const StandardMethodCodec().encodeMethodCall(
        const MethodCall('onKeyframeRequested', null),
      );
      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage('com.secondscreen/decoder_control', keyframeMethodCall, (data) {});

      await Future.delayed(Duration.zero);

      expect(fakeSocket.sentBytes.isNotEmpty, isTrue);
      final reqBytes = fakeSocket.sentBytes.last;
      expect(reqBytes.length, equals(24));
      final view = ByteData.sublistView(reqBytes);
      expect(view.getUint16(6, Endian.big), equals(0x0006)); // KeyframeRequest

      await service.disconnect();
      service.dispose();
      bridge.dispose();
    });

    test('sends upstream PACKET_TYPE_RECEIVER_REPORT during active SS02 streaming', () async {
      final fakeSocket = FakeSocket();
      final bridge = SecondScreenDecoderBridge();
      final service = DefaultConnectionService(
        socketConnector: (host, port, {timeout}) async => fakeSocket,
        decoderBridge: bridge,
      );

      await service.connectToDevice('192.168.1.100', 52025);

      // Codec config arrives
      final spsPps = Uint8List.fromList([0, 0, 0, 1, 0x67, 1, 0, 0, 0, 1, 0x68, 2]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.config, spsPps, sequenceNumber: 1));

      // Keyframe arrives -> streaming state
      final idrData = Uint8List.fromList([0, 0, 0, 1, 0x65, 88, 99]);
      fakeSocket.emitChunk(buildSs02Packet(Ss02PacketType.keyframe, idrData, sequenceNumber: 2));

      await Future.delayed(Duration.zero);
      expect(service.state.isStreaming, isTrue);

      // Send telemetry report
      final success = await service.sendReceiverReport(
        receivedFps: 30,
        decodedFps: 29,
        droppedFrames: 1,
        bufferDelayMs: 35,
      );
      expect(success, isTrue);
      expect(fakeSocket.sentBytes.isNotEmpty, isTrue);

      final reportBytes = fakeSocket.sentBytes.last;
      expect(reportBytes.length, equals(44));
      final view = ByteData.sublistView(reportBytes);
      expect(view.getUint16(6, Endian.big), equals(0x0008)); // ReceiverReport
      expect(view.getUint32(12, Endian.big), equals(20)); // payload length
      expect(view.getUint32(24, Endian.big), equals(30)); // receivedFps
      expect(view.getUint32(28, Endian.big), equals(29)); // decodedFps
      expect(view.getUint32(32, Endian.big), equals(1));  // droppedFrames
      expect(view.getUint32(36, Endian.big), equals(35)); // bufferDelayMs

      await service.disconnect();
      service.dispose();
      bridge.dispose();
    });

    group('Lifecycle Recovery and Automatic Reconnection', () {
      test('preserves target device and exposes lastConnectedDevice on unexpected disconnect', () async {
        final fakeSocket = FakeSocket();
        final service = DefaultConnectionService(
          socketConnector: (host, port, {timeout}) async => fakeSocket,
        );

        final connected = await service.connectToDevice('192.168.1.150', 52025, pcName: 'Work PC');
        expect(connected, isTrue);
        expect(service.isSocketConnected, isTrue);
        expect(service.lastConnectedDevice, isNotNull);
        expect(service.lastConnectedDevice!.address, equals('192.168.1.150'));
        expect(service.lastConnectedDevice!.name, equals('Work PC'));

        // Unexpected disconnect (e.g. Android suspended network, host dropped socket)
        await service.disconnect(reason: 'Connection dropped by host.');

        expect(service.state.status, equals(ConnectionStatus.failed));
        expect(service.state.errorMessage, equals('Connection dropped by host.'));
        // Target device is preserved in state and lastConnectedDevice
        expect(service.state.connectedDevice, isNotNull);
        expect(service.state.connectedDevice!.address, equals('192.168.1.150'));
        expect(service.lastConnectedDevice!.address, equals('192.168.1.150'));
        expect(service.isSocketConnected, isFalse);

        service.dispose();
      });

      test('reconnects with bounded exponential backoff when connection recovers', () async {
        int connectionAttempts = 0;
        final fakeSocket = FakeSocket();

        final service = DefaultConnectionService(
          socketConnector: (host, port, {timeout}) async {
            connectionAttempts++;
            if (connectionAttempts < 3) {
              throw const SocketException('Host unreachable');
            }
            return fakeSocket;
          },
        );

        // Initial connect fails
        await service.connectToDevice('192.168.1.150', 52025, pcName: 'Work PC');
        expect(service.state.status, equals(ConnectionStatus.failed));
        expect(service.lastConnectedDevice, isNotNull);

        // Reconnect retries with backoff and succeeds on 3rd attempt
        final reconnected = await service.reconnect(
          maxAttempts: 4,
          initialDelay: const Duration(milliseconds: 10),
        );

        expect(reconnected, isTrue);
        expect(connectionAttempts, equals(3));
        expect(service.state.status, equals(ConnectionStatus.connected));
        expect(service.isSocketConnected, isTrue);

        await service.disconnect();
        service.dispose();
      });

      test('prevents overlapping reconnection loops', () async {
        bool shouldBlock = false;
        final blocker = Completer<Socket>();
        final fakeSocket = FakeSocket();

        final service = DefaultConnectionService(
          socketConnector: (host, port, {timeout}) async {
            if (shouldBlock) {
              return blocker.future;
            }
            return fakeSocket;
          },
        );

        // Initial connect succeeds, establishing _lastConnectedDevice
        await service.connectToDevice('192.168.1.150', 52025);
        expect(service.lastConnectedDevice, isNotNull);
        expect(service.isSocketConnected, isTrue);

        // Drop connection
        await service.disconnect(reason: 'Network drop');
        expect(service.state.status, equals(ConnectionStatus.failed));

        // Subsequent reconnect attempts will await blocker.future
        shouldBlock = true;

        // First reconnect starts and blocks awaiting socket
        final firstFuture = service.reconnect(
          maxAttempts: 3,
          initialDelay: const Duration(milliseconds: 10),
        );
        expect(service.isReconnecting, isTrue);

        // Second reconnect while first is active must be rejected immediately
        final secondResult = await service.reconnect(maxAttempts: 3);
        expect(secondResult, isFalse);

        // Complete socket to unblock first attempt
        blocker.complete(FakeSocket());
        final firstResult = await firstFuture;
        expect(firstResult, isTrue);
        expect(service.isReconnecting, isFalse);

        await service.disconnect();
        service.dispose();
      });
    });
  });
}
