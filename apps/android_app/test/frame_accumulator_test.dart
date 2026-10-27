import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/frame_accumulator.dart';

Uint8List buildTestPacket({
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

void main() {
  group('FrameAccumulator Tests', () {
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
      final packet = buildTestPacket(
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
      final packet = buildTestPacket(payload: fakePayload);

      // Split into 3 arbitrary chunks: 10 bytes, 30 bytes, remaining bytes
      final chunk1 = Uint8List.sublistView(packet, 0, 10);
      final chunk2 = Uint8List.sublistView(packet, 10, 40);
      final chunk3 = Uint8List.sublistView(packet, 40);

      accumulator.addChunk(chunk1);
      expect(receivedFrames.isEmpty, isTrue); // Incomplete header

      accumulator.addChunk(chunk2);
      expect(
        receivedFrames.isEmpty,
        isTrue,
      ); // Header complete, payload partial

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
      builder.add(buildTestPacket(payload: payload1));
      builder.add(buildTestPacket(payload: payload2));

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

      final badPacket = buildTestPacket(
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

      // Construct a header claiming 20 MB payload
      final header = ByteData(20);
      header.setUint8(0, 0x53);
      header.setUint8(1, 0x53);
      header.setUint8(2, 0x30);
      header.setUint8(3, 0x31);
      header.setUint8(4, 0x01);
      header.setUint32(8, 20 * 1024 * 1024, Endian.big); // 20 MB

      accumulator.addChunk(header.buffer.asUint8List());

      expect(errorMessage, isNotNull);
      expect(errorMessage, contains('Invalid frame payload length'));
    });
  });
}
