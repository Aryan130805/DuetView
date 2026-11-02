#include "protocol/protocol_header.hpp"

namespace secondscreen::protocol {

bool ProtocolHeader::Serialize(std::span<uint8_t> outBuffer) const noexcept {
    if (outBuffer.size() < HEADER_SIZE) {
        return false;
    }

    // Magic: 'SS02'
    outBuffer[0] = static_cast<uint8_t>((magic >> 24) & 0xFFU);
    outBuffer[1] = static_cast<uint8_t>((magic >> 16) & 0xFFU);
    outBuffer[2] = static_cast<uint8_t>((magic >> 8) & 0xFFU);
    outBuffer[3] = static_cast<uint8_t>(magic & 0xFFU);

    // Version (uint16 BE)
    outBuffer[4] = static_cast<uint8_t>((version >> 8) & 0xFFU);
    outBuffer[5] = static_cast<uint8_t>(version & 0xFFU);

    // Packet Type (uint16 BE)
    const auto rawType = static_cast<uint16_t>(packetType);
    outBuffer[6] = static_cast<uint8_t>((rawType >> 8) & 0xFFU);
    outBuffer[7] = static_cast<uint8_t>(rawType & 0xFFU);

    // Sequence Number (uint32 BE)
    outBuffer[8] = static_cast<uint8_t>((sequenceNumber >> 24) & 0xFFU);
    outBuffer[9] = static_cast<uint8_t>((sequenceNumber >> 16) & 0xFFU);
    outBuffer[10] = static_cast<uint8_t>((sequenceNumber >> 8) & 0xFFU);
    outBuffer[11] = static_cast<uint8_t>(sequenceNumber & 0xFFU);

    // Payload Length (uint32 BE)
    outBuffer[12] = static_cast<uint8_t>((payloadLength >> 24) & 0xFFU);
    outBuffer[13] = static_cast<uint8_t>((payloadLength >> 16) & 0xFFU);
    outBuffer[14] = static_cast<uint8_t>((payloadLength >> 8) & 0xFFU);
    outBuffer[15] = static_cast<uint8_t>(payloadLength & 0xFFU);

    // Timestamp Us (uint64 BE)
    outBuffer[16] = static_cast<uint8_t>((timestampUs >> 56) & 0xFFU);
    outBuffer[17] = static_cast<uint8_t>((timestampUs >> 48) & 0xFFU);
    outBuffer[18] = static_cast<uint8_t>((timestampUs >> 40) & 0xFFU);
    outBuffer[19] = static_cast<uint8_t>((timestampUs >> 32) & 0xFFU);
    outBuffer[20] = static_cast<uint8_t>((timestampUs >> 24) & 0xFFU);
    outBuffer[21] = static_cast<uint8_t>((timestampUs >> 16) & 0xFFU);
    outBuffer[22] = static_cast<uint8_t>((timestampUs >> 8) & 0xFFU);
    outBuffer[23] = static_cast<uint8_t>(timestampUs & 0xFFU);

    return true;
}

ParseResult ProtocolHeader::Deserialize(std::span<const uint8_t> inBuffer, ProtocolHeader& outHeader) noexcept {
    if (inBuffer.size() < HEADER_SIZE) {
        return ParseResult::NeedMoreData;
    }

    // Magic
    const uint32_t parsedMagic =
        (static_cast<uint32_t>(inBuffer[0]) << 24) |
        (static_cast<uint32_t>(inBuffer[1]) << 16) |
        (static_cast<uint32_t>(inBuffer[2]) << 8) |
        static_cast<uint32_t>(inBuffer[3]);

    if (parsedMagic != PROTOCOL_MAGIC_U32) {
        return ParseResult::InvalidMagic;
    }

    // Version
    const uint16_t parsedVersion =
        static_cast<uint16_t>((static_cast<uint16_t>(inBuffer[4]) << 8) |
                              static_cast<uint16_t>(inBuffer[5]));

    if (parsedVersion != PROTOCOL_VERSION_2) {
        return ParseResult::UnsupportedVersion;
    }

    // Packet Type
    const uint16_t rawType =
        static_cast<uint16_t>((static_cast<uint16_t>(inBuffer[6]) << 8) |
                              static_cast<uint16_t>(inBuffer[7]));

    if (!IsValidPacketType(rawType)) {
        return ParseResult::UnknownPacketType;
    }

    const auto parsedType = static_cast<PacketType>(rawType);

    // Sequence Number
    const uint32_t parsedSeq =
        (static_cast<uint32_t>(inBuffer[8]) << 24) |
        (static_cast<uint32_t>(inBuffer[9]) << 16) |
        (static_cast<uint32_t>(inBuffer[10]) << 8) |
        static_cast<uint32_t>(inBuffer[11]);

    // Payload Length
    const uint32_t parsedPayloadLen =
        (static_cast<uint32_t>(inBuffer[12]) << 24) |
        (static_cast<uint32_t>(inBuffer[13]) << 16) |
        (static_cast<uint32_t>(inBuffer[14]) << 8) |
        static_cast<uint32_t>(inBuffer[15]);

    if (parsedPayloadLen > MAX_PAYLOAD_SIZE) {
        return ParseResult::PayloadTooLarge;
    }

    if (IsVideoPacket(parsedType) && parsedPayloadLen == 0U) {
        return ParseResult::ZeroLengthVideoPayload;
    }

    // Timestamp Us
    const uint64_t parsedTimestamp =
        (static_cast<uint64_t>(inBuffer[16]) << 56) |
        (static_cast<uint64_t>(inBuffer[17]) << 48) |
        (static_cast<uint64_t>(inBuffer[18]) << 40) |
        (static_cast<uint64_t>(inBuffer[19]) << 32) |
        (static_cast<uint64_t>(inBuffer[20]) << 24) |
        (static_cast<uint64_t>(inBuffer[21]) << 16) |
        (static_cast<uint64_t>(inBuffer[22]) << 8) |
        static_cast<uint64_t>(inBuffer[23]);

    outHeader.magic = parsedMagic;
    outHeader.version = parsedVersion;
    outHeader.packetType = parsedType;
    outHeader.sequenceNumber = parsedSeq;
    outHeader.payloadLength = parsedPayloadLen;
    outHeader.timestampUs = parsedTimestamp;

    return ParseResult::Success;
}

std::array<uint8_t, HEADER_SIZE> ProtocolHeader::ToBytes() const noexcept {
    std::array<uint8_t, HEADER_SIZE> bytes{};
    Serialize(bytes);
    return bytes;
}

} // namespace secondscreen::protocol
