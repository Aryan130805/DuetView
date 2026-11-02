#include "protocol/packet_parser.hpp"

namespace secondscreen::protocol {

PacketParser::PacketParser(size_t maxAccumulatorCapacity)
    : maxCapacity_(maxAccumulatorCapacity) {
    buffer_.reserve(64U * 1024U);
}

bool PacketParser::Consume(std::span<const uint8_t> chunk,
                           const PacketCallback& onPacket,
                           const ErrorCallback& onError) {
    if (chunk.empty()) {
        return true;
    }

    if (buffer_.size() + chunk.size() > maxCapacity_) {
        if (onError) {
            onError(ParseResult::PayloadTooLarge, "Accumulator capacity exceeded max allowed size");
        }
        buffer_.clear();
        return false;
    }

    buffer_.insert(buffer_.end(), chunk.begin(), chunk.end());

    while (buffer_.size() >= HEADER_SIZE) {
        ProtocolHeader header{};
        const std::span<const uint8_t> headerSpan(buffer_.data(), HEADER_SIZE);
        const ParseResult headerResult = ProtocolHeader::Deserialize(headerSpan, header);

        if (headerResult != ParseResult::Success) {
            if (onError) {
                onError(headerResult, ParseResultToString(headerResult));
            }
            buffer_.clear();
            return false;
        }

        const size_t totalPacketBytes = HEADER_SIZE + static_cast<size_t>(header.payloadLength);

        // Check if full payload has arrived
        if (buffer_.size() < totalPacketBytes) {
            // Need more data from socket
            break;
        }

        // Deliver full packet
        const std::span<const uint8_t> payloadSpan(buffer_.data() + HEADER_SIZE, header.payloadLength);
        if (onPacket) {
            onPacket(header, payloadSpan);
        }
        ++totalPacketsParsed_;

        // Erase consumed packet from accumulator
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(totalPacketBytes));
    }

    return true;
}

void PacketParser::Reset() noexcept {
    buffer_.clear();
    totalPacketsParsed_ = 0U;
}

} // namespace secondscreen::protocol
