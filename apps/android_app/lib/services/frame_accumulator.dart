import 'dart:typed_data';

/// SS02 packet types defined in shared/protocol/README.md
enum Ss02PacketType {
  config(0x0001),
  keyframe(0x0002),
  delta(0x0003),
  heartbeat(0x0004),
  heartbeatAck(0x0005),
  keyframeRequest(0x0006),
  shutdown(0x0007),
  receiverReport(0x0008),
  pairRequest(0x0009),
  pairResponse(0x000A),
  pointerEvent(0x000B),
  keyboardEvent(0x000C),
  scrollEvent(0x000D);

  final int value;
  const Ss02PacketType(this.value);

  static Ss02PacketType? fromValue(int val) {
    for (final t in values) {
      if (t.value == val) return t;
    }
    return null;
  }
}

/// Represents a parsed SS02 protocol packet.
class Ss02Packet {
  final Ss02PacketType type;
  final int sequenceNumber;
  final int payloadLength;
  final int timestampUs;
  final Uint8List payload;

  const Ss02Packet({
    required this.type,
    required this.sequenceNumber,
    required this.payloadLength,
    required this.timestampUs,
    required this.payload,
  });

  bool get isVideoPacket =>
      type == Ss02PacketType.config ||
      type == Ss02PacketType.keyframe ||
      type == Ss02PacketType.delta;
}

/// Serializes a 24-byte big-endian SS02 PACKET_TYPE_KEYFRAME_REQ (0x0006) message.
Uint8List createKeyframeRequestPacket({
  int sequenceNumber = 0,
  int timestampUs = 0,
}) {
  final header = ByteData(24);
  // Magic: 'S', 'S', '0', '2'
  header.setUint8(0, 0x53);
  header.setUint8(1, 0x53);
  header.setUint8(2, 0x30);
  header.setUint8(3, 0x32);

  // Version: 2
  header.setUint16(4, 2, Endian.big);

  // Packet Type: 0x0006
  header.setUint16(6, Ss02PacketType.keyframeRequest.value, Endian.big);

  // Sequence number
  header.setUint32(8, sequenceNumber, Endian.big);

  // Payload length: 0
  header.setUint32(12, 0, Endian.big);

  // Timestamp in microseconds
  header.setUint64(16, timestampUs, Endian.big);

  return header.buffer.asUint8List();
}

/// Serializes a 24-byte big-endian SS02 PACKET_TYPE_HEARTBEAT (0x0004) message.
Uint8List createHeartbeatPacket({int sequenceNumber = 0, int timestampUs = 0}) {
  final header = ByteData(24);
  header.setUint8(0, 0x53);
  header.setUint8(1, 0x53);
  header.setUint8(2, 0x30);
  header.setUint8(3, 0x32);
  header.setUint16(4, 2, Endian.big);
  header.setUint16(6, Ss02PacketType.heartbeat.value, Endian.big);
  header.setUint32(8, sequenceNumber, Endian.big);
  header.setUint32(12, 0, Endian.big);
  header.setUint64(16, timestampUs, Endian.big);
  return header.buffer.asUint8List();
}

/// Serializes a 24-byte big-endian SS02 PACKET_TYPE_HEARTBEAT_ACK (0x0005) message.
Uint8List createHeartbeatAckPacket({
  int sequenceNumber = 0,
  int timestampUs = 0,
}) {
  final header = ByteData(24);
  header.setUint8(0, 0x53);
  header.setUint8(1, 0x53);
  header.setUint8(2, 0x30);
  header.setUint8(3, 0x32);
  header.setUint16(4, 2, Endian.big);
  header.setUint16(6, Ss02PacketType.heartbeatAck.value, Endian.big);
  header.setUint32(8, sequenceNumber, Endian.big);
  header.setUint32(12, 0, Endian.big);
  header.setUint64(16, timestampUs, Endian.big);
  return header.buffer.asUint8List();
}

/// Serializes an SS02 PACKET_TYPE_PAIR_REQ (0x0009) message containing a pairing token.
Uint8List createPairRequestPacket({
  required int sequenceNumber,
  required int timestampUs,
  required String token,
  required String clientName,
}) {
  final payloadStr = '{"token":"$token","client":"$clientName"}';
  final payloadBytes = Uint8List.fromList(payloadStr.codeUnits);

  final buffer = Uint8List(24 + payloadBytes.length);
  final byteData = ByteData.sublistView(buffer);

  byteData.setUint8(0, 0x53);
  byteData.setUint8(1, 0x53);
  byteData.setUint8(2, 0x30);
  byteData.setUint8(3, 0x32);
  byteData.setUint16(4, 2, Endian.big);
  byteData.setUint16(6, Ss02PacketType.pairRequest.value, Endian.big);
  byteData.setUint32(8, sequenceNumber, Endian.big);
  byteData.setUint32(12, payloadBytes.length, Endian.big);
  byteData.setUint64(16, timestampUs, Endian.big);

  buffer.setRange(24, 24 + payloadBytes.length, payloadBytes);
  return buffer;
}

/// Serializes an SS02 PACKET_TYPE_PAIR_RESP (0x000A) message with status code (0=ok, 1=bad token, 2=expired).
Uint8List createPairResponsePacket({
  required int sequenceNumber,
  required int timestampUs,
  required int statusCode,
  String message = '',
}) {
  final payloadStr = '{"status":$statusCode,"message":"$message"}';
  final payloadBytes = Uint8List.fromList(payloadStr.codeUnits);

  final buffer = Uint8List(24 + payloadBytes.length);
  final byteData = ByteData.sublistView(buffer);

  byteData.setUint8(0, 0x53);
  byteData.setUint8(1, 0x53);
  byteData.setUint8(2, 0x30);
  byteData.setUint8(3, 0x32);
  byteData.setUint16(4, 2, Endian.big);
  byteData.setUint16(6, Ss02PacketType.pairResponse.value, Endian.big);
  byteData.setUint32(8, sequenceNumber, Endian.big);
  byteData.setUint32(12, payloadBytes.length, Endian.big);
  byteData.setUint64(16, timestampUs, Endian.big);

  buffer.setRange(24, 24 + payloadBytes.length, payloadBytes);
  return buffer;
}

/// Serializes a 44-byte big-endian SS02 PACKET_TYPE_RECEIVER_REPORT (0x0008) message
/// containing a 24-byte header and a 20-byte telemetry payload.
Uint8List createReceiverReportPacket({
  required int sequenceNumber,
  required int timestampUs,
  required int receivedFps,
  required int decodedFps,
  required int droppedFrames,
  required int bufferDelayMs,
  int flags = 0,
}) {
  final buffer = Uint8List(24 + 20);
  final byteData = ByteData.sublistView(buffer);

  // 1. Magic 'SS02'
  byteData.setUint8(0, 0x53);
  byteData.setUint8(1, 0x53);
  byteData.setUint8(2, 0x30);
  byteData.setUint8(3, 0x32);

  // 2. Version 2 (uint16)
  byteData.setUint16(4, 2, Endian.big);

  // 3. Packet Type: 0x0008 (uint16)
  byteData.setUint16(6, Ss02PacketType.receiverReport.value, Endian.big);

  // 4. Sequence number (uint32)
  byteData.setUint32(8, sequenceNumber, Endian.big);

  // 5. Payload length: 20 bytes (uint32)
  byteData.setUint32(12, 20, Endian.big);

  // 6. Timestamp in microseconds (uint64)
  byteData.setUint64(16, timestampUs, Endian.big);

  // 7. Payload (20 bytes)
  byteData.setUint32(24, receivedFps, Endian.big);
  byteData.setUint32(28, decodedFps, Endian.big);
  byteData.setUint32(32, droppedFrames, Endian.big);
  byteData.setUint32(36, bufferDelayMs, Endian.big);
  byteData.setUint32(40, flags, Endian.big);

  return buffer;
}

/// Pointer event action types for SS02 PACKET_TYPE_POINTER_EVENT
enum PointerEventType {
  move(0x0001),
  buttonDown(0x0002),
  buttonUp(0x0003),
  doubleClick(0x0004);

  final int value;
  const PointerEventType(this.value);
}

/// Pointer buttons for SS02 PACKET_TYPE_POINTER_EVENT
enum PointerButton {
  none(0x00),
  left(0x01),
  right(0x02),
  middle(0x03);

  final int value;
  const PointerButton(this.value);
}

/// Keyboard event action types for SS02 PACKET_TYPE_KEYBOARD_EVENT
enum KeyEventType {
  keyDown(0x0001),
  keyUp(0x0002),
  unicodeChar(0x0003);

  final int value;
  const KeyEventType(this.value);
}

/// Modifier bitmask flags
class ModifierFlags {
  static const int none = 0x00;
  static const int shift = 0x01;
  static const int ctrl = 0x02;
  static const int alt = 0x04;
  static const int meta = 0x08;
}

/// Serializes an SS02 PACKET_TYPE_POINTER_EVENT (0x000B) message (24-byte header + 16-byte payload = 40 bytes).
Uint8List createPointerEventPacket({
  required int sequenceNumber,
  required int timestampUs,
  required PointerEventType type,
  PointerButton button = PointerButton.none,
  int modifiers = 0,
  required double normalizedX,
  required double normalizedY,
  int flags = 0,
}) {
  final buffer = Uint8List(40);
  final byteData = ByteData.sublistView(buffer);

  // Header (24 bytes)
  byteData.setUint8(0, 0x53); // 'S'
  byteData.setUint8(1, 0x53); // 'S'
  byteData.setUint8(2, 0x30); // '0'
  byteData.setUint8(3, 0x32); // '2'
  byteData.setUint16(4, 2, Endian.big);
  byteData.setUint16(6, Ss02PacketType.pointerEvent.value, Endian.big);
  byteData.setUint32(8, sequenceNumber, Endian.big);
  byteData.setUint32(12, 16, Endian.big); // 16-byte payload
  byteData.setUint64(16, timestampUs, Endian.big);

  // Payload (16 bytes)
  byteData.setUint16(24, type.value, Endian.big);
  byteData.setUint8(26, button.value);
  byteData.setUint8(27, modifiers & 0xFF);
  byteData.setFloat32(28, normalizedX.clamp(0.0, 1.0), Endian.big);
  byteData.setFloat32(32, normalizedY.clamp(0.0, 1.0), Endian.big);
  byteData.setUint32(36, flags, Endian.big);

  return buffer;
}

/// Serializes an SS02 PACKET_TYPE_SCROLL_EVENT (0x000D) message (24-byte header + 16-byte payload = 40 bytes).
Uint8List createScrollEventPacket({
  required int sequenceNumber,
  required int timestampUs,
  required double normalizedX,
  required double normalizedY,
  int deltaX = 0,
  required int deltaY,
}) {
  final buffer = Uint8List(40);
  final byteData = ByteData.sublistView(buffer);

  // Header (24 bytes)
  byteData.setUint8(0, 0x53);
  byteData.setUint8(1, 0x53);
  byteData.setUint8(2, 0x30);
  byteData.setUint8(3, 0x32);
  byteData.setUint16(4, 2, Endian.big);
  byteData.setUint16(6, Ss02PacketType.scrollEvent.value, Endian.big);
  byteData.setUint32(8, sequenceNumber, Endian.big);
  byteData.setUint32(12, 16, Endian.big);
  byteData.setUint64(16, timestampUs, Endian.big);

  // Payload (16 bytes)
  byteData.setFloat32(24, normalizedX.clamp(0.0, 1.0), Endian.big);
  byteData.setFloat32(28, normalizedY.clamp(0.0, 1.0), Endian.big);
  byteData.setInt32(32, deltaX, Endian.big);
  byteData.setInt32(36, deltaY, Endian.big);

  return buffer;
}

/// Serializes an SS02 PACKET_TYPE_KEYBOARD_EVENT (0x000C) message (24-byte header + 12-byte payload = 36 bytes).
Uint8List createKeyboardEventPacket({
  required int sequenceNumber,
  required int timestampUs,
  required KeyEventType type,
  int modifiers = 0,
  int keyCode = 0,
  int unicodeCodepoint = 0,
}) {
  final buffer = Uint8List(36);
  final byteData = ByteData.sublistView(buffer);

  // Header (24 bytes)
  byteData.setUint8(0, 0x53);
  byteData.setUint8(1, 0x53);
  byteData.setUint8(2, 0x30);
  byteData.setUint8(3, 0x32);
  byteData.setUint16(4, 2, Endian.big);
  byteData.setUint16(6, Ss02PacketType.keyboardEvent.value, Endian.big);
  byteData.setUint32(8, sequenceNumber, Endian.big);
  byteData.setUint32(12, 12, Endian.big);
  byteData.setUint64(16, timestampUs, Endian.big);

  // Payload (12 bytes)
  byteData.setUint16(24, type.value, Endian.big);
  byteData.setUint8(26, modifiers & 0xFF);
  byteData.setUint8(27, 0); // reserved
  byteData.setUint32(28, keyCode, Endian.big);
  byteData.setUint32(32, unicodeCodepoint, Endian.big);

  return buffer;
}

/// Handles chunk reassembly and packet framing for both:
/// - SecondScreen v2 (SS02) H.264 video streaming protocol (24-byte header)
/// - SecondScreen v1 (SS01) JPEG streaming fallback protocol (20-byte header)
class FrameAccumulator {
  static const int ss01HeaderSize = 20;
  static const int ss02HeaderSize = 24;

  static const int maxSs01PayloadSize = 10 * 1024 * 1024; // 10 MB limit
  static const int maxSs02PayloadSize = 16 * 1024 * 1024; // 16 MB limit

  static const List<int> ss01Magic = [0x53, 0x53, 0x30, 0x31]; // 'SS01'
  static const List<int> ss02Magic = [0x53, 0x53, 0x30, 0x32]; // 'SS02'

  final void Function(Uint8List frameBytes, int timestampMs)? onFrame;
  final void Function(Ss02Packet packet)? onSs02Packet;
  final void Function(String error)? onError;

  final BytesBuilder _buffer = BytesBuilder(copy: false);

  FrameAccumulator({this.onFrame, this.onSs02Packet, this.onError});

  /// Appends incoming TCP socket bytes and extracts any complete frames or packets.
  void addChunk(Uint8List chunk) {
    if (chunk.isEmpty) return;
    _buffer.add(chunk);

    _processBuffer();
  }

  void _processBuffer() {
    Uint8List currentBytes = _buffer.toBytes();
    int offset = 0;

    while (currentBytes.length - offset >= 4) {
      // Inspect first 4 bytes to determine protocol version
      final m0 = currentBytes[offset];
      final m1 = currentBytes[offset + 1];
      final m2 = currentBytes[offset + 2];
      final m3 = currentBytes[offset + 3];

      final isSs02 =
          m0 == ss02Magic[0] &&
          m1 == ss02Magic[1] &&
          m2 == ss02Magic[2] &&
          m3 == ss02Magic[3];

      final isSs01 =
          m0 == ss01Magic[0] &&
          m1 == ss01Magic[1] &&
          m2 == ss01Magic[2] &&
          m3 == ss01Magic[3];

      if (!isSs02 && !isSs01) {
        // Resilient framing recovery: search forward for next 'SS02' or 'SS01' sync marker
        int nextSync = -1;
        for (int i = offset + 1; i <= currentBytes.length - 4; i++) {
          if (currentBytes[i] == 0x53 &&
              currentBytes[i + 1] == 0x53 &&
              currentBytes[i + 2] == 0x30 &&
              (currentBytes[i + 3] == 0x31 || currentBytes[i + 3] == 0x32)) {
            nextSync = i;
            break;
          }
        }

        if (nextSync != -1) {
          offset = nextSync;
          continue;
        } else {
          onError?.call('Protocol error: Invalid packet magic bytes.');
          _buffer.clear();
          return;
        }
      }

      if (isSs02) {
        // --- Process SS02 Packet (24-byte header) ---
        if (currentBytes.length - offset < ss02HeaderSize) {
          // Incomplete SS02 header: wait for more bytes
          break;
        }

        final headerView = ByteData.sublistView(
          currentBytes,
          offset,
          offset + ss02HeaderSize,
        );

        final version = headerView.getUint16(4, Endian.big);
        if (version != 2) {
          onError?.call('Protocol error: Unsupported SS02 version $version.');
          _buffer.clear();
          return;
        }

        final rawType = headerView.getUint16(6, Endian.big);
        final packetType = Ss02PacketType.fromValue(rawType);
        if (packetType == null) {
          onError?.call('Protocol error: Unknown SS02 packet type $rawType.');
          _buffer.clear();
          return;
        }

        final seqNum = headerView.getUint32(8, Endian.big);
        final payloadLength = headerView.getUint32(12, Endian.big);
        final timestampUs = headerView.getUint64(16, Endian.big);

        if (payloadLength > maxSs02PayloadSize) {
          onError?.call(
            'Protocol error: SS02 payload exceeds maximum limit ($payloadLength bytes).',
          );
          _buffer.clear();
          return;
        }

        // Video packets must have payload > 0
        final isVideo =
            packetType == Ss02PacketType.config ||
            packetType == Ss02PacketType.keyframe ||
            packetType == Ss02PacketType.delta;

        if (isVideo && payloadLength == 0) {
          onError?.call(
            'Protocol error: SS02 video packet $packetType has 0 payload length.',
          );
          _buffer.clear();
          return;
        }

        final totalPacketSize = ss02HeaderSize + payloadLength;
        final availableBytes = currentBytes.length - offset;

        if (availableBytes < totalPacketSize) {
          // Incomplete SS02 payload: wait for subsequent chunks
          break;
        }

        final payload = Uint8List.sublistView(
          currentBytes,
          offset + ss02HeaderSize,
          offset + totalPacketSize,
        );

        final packet = Ss02Packet(
          type: packetType,
          sequenceNumber: seqNum,
          payloadLength: payloadLength,
          timestampUs: timestampUs,
          payload: payload,
        );

        onSs02Packet?.call(packet);
        offset += totalPacketSize;
      } else {
        // --- Process SS01 Packet (20-byte header) ---
        if (currentBytes.length - offset < ss01HeaderSize) {
          // Incomplete SS01 header: wait for more bytes
          break;
        }

        final headerView = ByteData.sublistView(
          currentBytes,
          offset,
          offset + ss01HeaderSize,
        );

        final payloadLength = headerView.getUint32(8, Endian.big);
        if (payloadLength == 0 || payloadLength > maxSs01PayloadSize) {
          onError?.call(
            'Protocol error: Invalid SS01 frame payload length ($payloadLength bytes).',
          );
          _buffer.clear();
          return;
        }

        final timestampMs = headerView.getUint64(12, Endian.big);
        final totalPacketSize = ss01HeaderSize + payloadLength;
        final availableBytes = currentBytes.length - offset;

        if (availableBytes < totalPacketSize) {
          // Incomplete SS01 payload: wait for subsequent chunks
          break;
        }

        final frameBytes = Uint8List.sublistView(
          currentBytes,
          offset + ss01HeaderSize,
          offset + totalPacketSize,
        );

        onFrame?.call(frameBytes, timestampMs);
        offset += totalPacketSize;
      }
    }

    // Retain only unconsumed trailing bytes in buffer
    _buffer.clear();
    if (offset < currentBytes.length) {
      _buffer.add(Uint8List.sublistView(currentBytes, offset));
    }
  }

  /// Clears accumulated bytes and resets state.
  void reset() {
    _buffer.clear();
  }
}
