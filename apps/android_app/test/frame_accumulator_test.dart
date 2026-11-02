import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';

Uint8List buildSs01TestPacket({
  required Uint8List payload,
  int timestampMs = 1700000000000,
  List<int>? magic,
}) {
  final header = ByteData(20);
  final m = magic ?? [0x53, 0x53, 0x30, 0x31]; // 'SS01'
  header.setUint8(0, m[0]);
  header.setUint8(1, m[1]);
  header.setUint8(2, m[2]);
  header.setUint8(3, m[3]);

  header.setUint8(4, 0x01); // Type: Frame
  header.setUint8(5, 0x00);
  header.setUint8(6, 0x00);
  header.setUint8(7, 0x00);

  header.setUint32(8, payload.length, Endian.big);
  header.setUint64(12, timestampMs, Endian.big);

  final builder = BytesBuilder();
  builder.add(header.buffer.asUint8List());
  builder.add(payload);
  return builder.toBytes();
}

Uint8List buildSs02TestPacket({
  required Ss02PacketType type,
  required Uint8List payload,
  int sequenceNumber = 1,
  int timestampUs = 1000000,
  int version = 2,
  List<int>? magic,
}) {
  final header = ByteData(24);
  final m = magic ?? [0x53, 0x53, 0x30, 0x32]; // 'SS02'
  header.setUint8(0, m[0]);
  header.setUint8(1, m[1]);
  header.setUint8(2, m[2]);
  header.setUint8(3, m[3]);

  header.setUint16(4, version, Endian.big);
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
  group('FrameAccumulator Phase 3 SS01 Tests', () {
    test('extracts single frame from complete chunk', () {
      final receivedFrames = <Uint8List>[];
      final receivedTimestamps = <int>[];

      final accumulator = FrameAccumulator(
        onFrame: (frame, ts) {
          receivedFrames.add(frame);
          receivedTimestamps.add(ts);
        },
      );

      final fakePayload = Uint8List.fromList([
        0xFF,
        0xD8,
        0x01,
        0x02,
        0xFF,
        0xD9,
      ]);
      final packet = buildSs01TestPacket(
        payload: fakePayload,
        timestampMs: 123456789,
      );

      accumulator.addChunk(packet);

      expect(receivedFrames.length, 1);
      expect(receivedFrames.first, fakePayload);
      expect(receivedTimestamps.first, 123456789);
    });

    test('reassembles fragmented TCP packets across multiple chunks', () {
      final receivedFrames = <Uint8List>[];

      final accumulator = FrameAccumulator(
        onFrame: (frame, ts) => receivedFrames.add(frame),
      );

      final fakePayload = Uint8List.fromList(List.generate(100, (i) => i));
      final packet = buildSs01TestPacket(payload: fakePayload);

      final chunk1 = Uint8List.sublistView(packet, 0, 10);
      final chunk2 = Uint8List.sublistView(packet, 10, 40);
      final chunk3 = Uint8List.sublistView(packet, 40);

      accumulator.addChunk(chunk1);
      expect(receivedFrames.isEmpty, isTrue);

      accumulator.addChunk(chunk2);
      expect(receivedFrames.isEmpty, isTrue);

      accumulator.addChunk(chunk3);
      expect(receivedFrames.length, 1);
      expect(receivedFrames.first, fakePayload);
    });

    test('extracts multiple frames arriving in a single chunk', () {
      final receivedFrames = <Uint8List>[];

      final accumulator = FrameAccumulator(
        onFrame: (frame, ts) => receivedFrames.add(frame),
      );

      final payload1 = Uint8List.fromList([0x01, 0x02, 0x03]);
      final payload2 = Uint8List.fromList([0x04, 0x05, 0x06, 0x07]);

      final builder = BytesBuilder();
      builder.add(buildSs01TestPacket(payload: payload1));
      builder.add(buildSs01TestPacket(payload: payload2));

      accumulator.addChunk(builder.toBytes());

      expect(receivedFrames.length, 2);
      expect(receivedFrames[0], payload1);
      expect(receivedFrames[1], payload2);
    });

    test('reports error on invalid magic bytes', () {
      String? errorMessage;

      final accumulator = FrameAccumulator(
        onFrame: (_, _) {},
        onError: (err) => errorMessage = err,
      );

      final badPacket = buildSs01TestPacket(
        payload: Uint8List(10),
        magic: [0x42, 0x41, 0x44, 0x21], // 'BAD!'
      );

      accumulator.addChunk(badPacket);

      expect(errorMessage, isNotNull);
      expect(errorMessage, contains('Invalid packet magic bytes'));
    });

    test('reports error on oversized frame length (> 10MB)', () {
      String? errorMessage;

      final accumulator = FrameAccumulator(
        onFrame: (_, _) {},
        onError: (err) => errorMessage = err,
      );

      final header = ByteData(20);
      header.setUint8(0, 0x53);
      header.setUint8(1, 0x53);
      header.setUint8(2, 0x30);
      header.setUint8(3, 0x31);
      header.setUint8(4, 0x01);
      header.setUint32(8, 20 * 1024 * 1024, Endian.big); // 20 MB

      accumulator.addChunk(header.buffer.asUint8List());

      expect(errorMessage, isNotNull);
      expect(errorMessage, contains('Invalid SS01 frame payload length'));
    });
  });

  group('FrameAccumulator Phase 4 SS02 Tests', () {
    test('extracts single SS02 packet from complete chunk', () {
      final packets = <Ss02Packet>[];

      final accumulator = FrameAccumulator(
        onSs02Packet: packets.add,
      );

      final spsPps = Uint8List.fromList([0x00, 0x00, 0x00, 0x01, 0x67, 0x42]);
      final packetBytes = buildSs02TestPacket(
        type: Ss02PacketType.config,
        payload: spsPps,
        sequenceNumber: 10,
        timestampUs: 987654321,
      );

      accumulator.addChunk(packetBytes);

      expect(packets.length, 1);
      expect(packets.first.type, equals(Ss02PacketType.config));
      expect(packets.first.sequenceNumber, equals(10));
      expect(packets.first.timestampUs, equals(987654321));
      expect(packets.first.payload, equals(spsPps));
      expect(packets.first.payloadLength, equals(spsPps.length));
    });

    test('handles SS02 header split across multiple TCP reads', () {
      final packets = <Ss02Packet>[];

      final accumulator = FrameAccumulator(
        onSs02Packet: packets.add,
      );

      final idrPayload = Uint8List.fromList([0x00, 0x00, 0x00, 0x01, 0x65, 0x88]);
      final packetBytes = buildSs02TestPacket(
        type: Ss02PacketType.keyframe,
        payload: idrPayload,
      );

      // Split 24-byte header into 11 bytes, 8 bytes, and remainder
      final chunk1 = Uint8List.sublistView(packetBytes, 0, 11);
      final chunk2 = Uint8List.sublistView(packetBytes, 11, 19);
      final chunk3 = Uint8List.sublistView(packetBytes, 19);

      accumulator.addChunk(chunk1);
      expect(packets.isEmpty, isTrue);

      accumulator.addChunk(chunk2);
      expect(packets.isEmpty, isTrue);

      accumulator.addChunk(chunk3);
      expect(packets.length, 1);
      expect(packets.first.type, equals(Ss02PacketType.keyframe));
      expect(packets.first.payload, equals(idrPayload));
    });

    test('handles SS02 payload split across multiple TCP reads', () {
      final packets = <Ss02Packet>[];

      final accumulator = FrameAccumulator(
        onSs02Packet: packets.add,
      );

      final payload = Uint8List.fromList(List.generate(200, (i) => (i * 3) & 0xFF));
      final packetBytes = buildSs02TestPacket(
        type: Ss02PacketType.delta,
        payload: payload,
      );

      // Header + first 50 bytes of payload
      final chunk1 = Uint8List.sublistView(packetBytes, 0, 24 + 50);
      // Next 100 bytes of payload
      final chunk2 = Uint8List.sublistView(packetBytes, 24 + 50, 24 + 150);
      // Final 50 bytes of payload
      final chunk3 = Uint8List.sublistView(packetBytes, 24 + 150);

      accumulator.addChunk(chunk1);
      expect(packets.isEmpty, isTrue);

      accumulator.addChunk(chunk2);
      expect(packets.isEmpty, isTrue);

      accumulator.addChunk(chunk3);
      expect(packets.length, 1);
      expect(packets.first.type, equals(Ss02PacketType.delta));
      expect(packets.first.payload, equals(payload));
    });

    test('extracts CONFIG, KEYFRAME, and DELTA arriving in single chunk', () {
      final packets = <Ss02Packet>[];

      final accumulator = FrameAccumulator(
        onSs02Packet: packets.add,
      );

      final configPayload = Uint8List.fromList([0, 0, 0, 1, 0x67, 1]);
      final keyframePayload = Uint8List.fromList([0, 0, 0, 1, 0x65, 2]);
      final deltaPayload = Uint8List.fromList([0, 0, 0, 1, 0x41, 3]);

      final builder = BytesBuilder();
      builder.add(buildSs02TestPacket(type: Ss02PacketType.config, payload: configPayload, sequenceNumber: 1));
      builder.add(buildSs02TestPacket(type: Ss02PacketType.keyframe, payload: keyframePayload, sequenceNumber: 2));
      builder.add(buildSs02TestPacket(type: Ss02PacketType.delta, payload: deltaPayload, sequenceNumber: 3));

      accumulator.addChunk(builder.toBytes());

      expect(packets.length, 3);
      expect(packets[0].type, equals(Ss02PacketType.config));
      expect(packets[0].sequenceNumber, equals(1));
      expect(packets[1].type, equals(Ss02PacketType.keyframe));
      expect(packets[1].sequenceNumber, equals(2));
      expect(packets[2].type, equals(Ss02PacketType.delta));
      expect(packets[2].sequenceNumber, equals(3));
    });

    test('rejects SS02 with invalid version', () {
      String? error;
      final accumulator = FrameAccumulator(
        onSs02Packet: (_) {},
        onError: (err) => error = err,
      );

      final badVersionPacket = buildSs02TestPacket(
        type: Ss02PacketType.config,
        payload: Uint8List.fromList([1, 2, 3]),
        version: 99,
      );

      accumulator.addChunk(badVersionPacket);
      expect(error, contains('Unsupported SS02 version 99'));
    });

    test('rejects SS02 with unknown packet type', () {
      String? error;
      final accumulator = FrameAccumulator(
        onSs02Packet: (_) {},
        onError: (err) => error = err,
      );

      // Packet with type 0x00FF
      final rawHeader = ByteData(24);
      rawHeader.setUint8(0, 0x53);
      rawHeader.setUint8(1, 0x53);
      rawHeader.setUint8(2, 0x30);
      rawHeader.setUint8(3, 0x32);
      rawHeader.setUint16(4, 2, Endian.big);
      rawHeader.setUint16(6, 0x00FF, Endian.big);
      rawHeader.setUint32(8, 1, Endian.big);
      rawHeader.setUint32(12, 4, Endian.big);
      rawHeader.setUint64(16, 0, Endian.big);

      final builder = BytesBuilder();
      builder.add(rawHeader.buffer.asUint8List());
      builder.add(Uint8List(4));

      accumulator.addChunk(builder.toBytes());
      expect(error, contains('Unknown SS02 packet type'));
    });

    test('rejects SS02 video packet with zero length', () {
      String? error;
      final accumulator = FrameAccumulator(
        onSs02Packet: (_) {},
        onError: (err) => error = err,
      );

      final header = ByteData(24);
      header.setUint8(0, 0x53);
      header.setUint8(1, 0x53);
      header.setUint8(2, 0x30);
      header.setUint8(3, 0x32);
      header.setUint16(4, 2, Endian.big);
      header.setUint16(6, Ss02PacketType.keyframe.value, Endian.big);
      header.setUint32(8, 1, Endian.big);
      header.setUint32(12, 0, Endian.big); // 0 length for video packet
      header.setUint64(16, 0, Endian.big);

      accumulator.addChunk(header.buffer.asUint8List());
      expect(error, contains('video packet'));
      expect(error, contains('has 0 payload length'));
    });

    test('rejects SS02 packet with payload length > 16MB', () {
      String? error;
      final accumulator = FrameAccumulator(
        onSs02Packet: (_) {},
        onError: (err) => error = err,
      );

      final header = ByteData(24);
      header.setUint8(0, 0x53);
      header.setUint8(1, 0x53);
      header.setUint8(2, 0x30);
      header.setUint8(3, 0x32);
      header.setUint16(4, 2, Endian.big);
      header.setUint16(6, Ss02PacketType.delta.value, Endian.big);
      header.setUint32(8, 1, Endian.big);
      header.setUint32(12, 20 * 1024 * 1024, Endian.big); // 20 MB
      header.setUint64(16, 0, Endian.big);

      accumulator.addChunk(header.buffer.asUint8List());
      expect(error, contains('payload exceeds maximum limit'));
    });

    test('createKeyframeRequestPacket generates valid 24-byte SS02 message', () {
      final req = createKeyframeRequestPacket(
        sequenceNumber: 777,
        timestampUs: 1234567890,
      );

      expect(req.length, equals(24));
      final view = ByteData.sublistView(req);

      // Magic
      expect(view.getUint8(0), equals(0x53));
      expect(view.getUint8(1), equals(0x53));
      expect(view.getUint8(2), equals(0x30));
      expect(view.getUint8(3), equals(0x32));

      // Version
      expect(view.getUint16(4, Endian.big), equals(2));

      // Type: KeyframeRequest (0x0006)
      expect(view.getUint16(6, Endian.big), equals(0x0006));

      // Sequence
      expect(view.getUint32(8, Endian.big), equals(777));

      // Payload length: 0
      expect(view.getUint32(12, Endian.big), equals(0));

      // Timestamp
      expect(view.getUint64(16, Endian.big), equals(1234567890));
    });

    test('createReceiverReportPacket generates valid 44-byte SS02 message', () {
      final report = createReceiverReportPacket(
        sequenceNumber: 888,
        timestampUs: 987654321,
        receivedFps: 30,
        decodedFps: 28,
        droppedFrames: 3,
        bufferDelayMs: 45,
      );

      expect(report.length, equals(44));
      final view = ByteData.sublistView(report);

      // Header checks
      expect(view.getUint8(0), equals(0x53));
      expect(view.getUint8(1), equals(0x53));
      expect(view.getUint8(2), equals(0x30));
      expect(view.getUint8(3), equals(0x32));
      expect(view.getUint16(4, Endian.big), equals(2));
      expect(view.getUint16(6, Endian.big), equals(0x0008)); // ReceiverReport
      expect(view.getUint32(8, Endian.big), equals(888));
      expect(view.getUint32(12, Endian.big), equals(20)); // 20-byte payload
      expect(view.getUint64(16, Endian.big), equals(987654321));

      // Telemetry payload checks
      expect(view.getUint32(24, Endian.big), equals(30)); // receivedFps
      expect(view.getUint32(28, Endian.big), equals(28)); // decodedFps
      expect(view.getUint32(32, Endian.big), equals(3));  // droppedFrames
      expect(view.getUint32(36, Endian.big), equals(45)); // bufferDelayMs
      expect(view.getUint32(40, Endian.big), equals(0));  // flags
    });
  });
}
