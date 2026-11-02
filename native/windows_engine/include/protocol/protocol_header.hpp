#pragma once

#include "protocol/protocol_constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace secondscreen::protocol {

struct ProtocolHeader {
    uint32_t magic{PROTOCOL_MAGIC_U32};
    uint16_t version{PROTOCOL_VERSION_2};
    PacketType packetType{PacketType::Heartbeat};
    uint32_t sequenceNumber{0U};
    uint32_t payloadLength{0U};
    uint64_t timestampUs{0U};

    [[nodiscard]] bool operator==(const ProtocolHeader& other) const noexcept = default;

    /// Serializes this header into a 24-byte buffer in big-endian network byte order.
    /// Returns true on success, false if buffer size is less than HEADER_SIZE.
    bool Serialize(std::span<uint8_t> outBuffer) const noexcept;

    /// Deserializes a 24-byte buffer into a ProtocolHeader without allocating heap memory.
    /// Validates magic, version, packet type, and payload size bounds.
    static ParseResult Deserialize(std::span<const uint8_t> inBuffer, ProtocolHeader& outHeader) noexcept;

    /// Convenience helper to create a serialized 24-byte array directly.
    [[nodiscard]] std::array<uint8_t, HEADER_SIZE> ToBytes() const noexcept;
};

} // namespace secondscreen::protocol
