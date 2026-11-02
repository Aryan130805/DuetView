#pragma once

#include "protocol/protocol_constants.hpp"
#include "protocol/protocol_header.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace secondscreen::protocol {

struct Packet {
    ProtocolHeader header;
    std::vector<uint8_t> payload;
};

using PacketCallback = std::function<void(const ProtocolHeader& header, std::span<const uint8_t> payload)>;
using ErrorCallback = std::function<void(ParseResult error, const char* message)>;

class PacketParser {
public:
    explicit PacketParser(size_t maxAccumulatorCapacity = MAX_PAYLOAD_SIZE + HEADER_SIZE);

    /// Appends incoming socket chunk bytes and processes any complete packets available.
    /// Returns true if stream remains healthy, false on fatal protocol violation (invalid magic, etc.).
    bool Consume(std::span<const uint8_t> chunk,
                 const PacketCallback& onPacket,
                 const ErrorCallback& onError);

    /// Resets accumulator state (e.g. after disconnect).
    void Reset() noexcept;

    /// Current bytes waiting in the accumulator.
    [[nodiscard]] size_t BufferedBytes() const noexcept { return buffer_.size(); }

    /// Total complete packets parsed.
    [[nodiscard]] uint64_t TotalPacketsParsed() const noexcept { return totalPacketsParsed_; }

private:
    std::vector<uint8_t> buffer_;
    size_t maxCapacity_;
    uint64_t totalPacketsParsed_{0U};
};

} // namespace secondscreen::protocol
