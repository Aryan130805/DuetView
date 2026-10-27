import 'dart:typed_data';

/// Handles chunk reassembly and packet framing for the SecondScreen TCP streaming protocol (v1).
///
/// Header structure (20 bytes big-endian):
/// - 4 bytes: Magic (ASCII 'SS01')
/// - 1 byte:  Type (0x01 = Frame)
/// - 3 bytes: Reserved (0x00 0x00 0x00)
/// - 4 bytes: Payload Length (uint32)
/// - 8 bytes: Host timestamp in ms (uint64)
class FrameAccumulator {
  static const int headerSize = 20;
  static const int maxPayloadSize = 10 * 1024 * 1024; // 10 MB limit
  static const List<int> magicBytes = [0x53, 0x53, 0x30, 0x31]; // 'SS01'

  final void Function(Uint8List frameBytes, int timestampMs) onFrame;
  final void Function(String error)? onError;

  final BytesBuilder _buffer = BytesBuilder(copy: false);

  FrameAccumulator({required this.onFrame, this.onError});

  /// Appends incoming TCP socket bytes and extracts any complete frames.
  void addChunk(Uint8List chunk) {
    if (chunk.isEmpty) return;
    _buffer.add(chunk);

    _processBuffer();
  }

  void _processBuffer() {
    Uint8List currentBytes = _buffer.toBytes();
    int offset = 0;

    while (currentBytes.length - offset >= headerSize) {
      final headerView = ByteData.sublistView(
        currentBytes,
        offset,
        offset + headerSize,
      );

      // 1. Verify Magic Bytes ('SS01')
      final m0 = headerView.getUint8(0);
      final m1 = headerView.getUint8(1);
      final m2 = headerView.getUint8(2);
      final m3 = headerView.getUint8(3);

      if (m0 != magicBytes[0] ||
          m1 != magicBytes[1] ||
          m2 != magicBytes[2] ||
          m3 != magicBytes[3]) {
        onError?.call('Protocol error: Invalid packet magic bytes.');
        _buffer.clear();
        return;
      }

      // 2. Read Payload Length
      final payloadLength = headerView.getUint32(8, Endian.big);
      if (payloadLength == 0 || payloadLength > maxPayloadSize) {
        onError?.call(
          'Protocol error: Invalid frame payload length ($payloadLength bytes).',
        );
        _buffer.clear();
        return;
      }

      // 3. Read Timestamp (64-bit uint)
      final timestampMs = headerView.getUint64(12, Endian.big);

      final totalPacketSize = headerSize + payloadLength;
      final availableBytes = currentBytes.length - offset;

      // Incomplete packet: wait for subsequent chunks
      if (availableBytes < totalPacketSize) {
        break;
      }

      // 4. Complete packet extracted
      final frameBytes = Uint8List.sublistView(
        currentBytes,
        offset + headerSize,
        offset + totalPacketSize,
      );

      onFrame(frameBytes, timestampMs);

      offset += totalPacketSize;
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
