#pragma once

#include <cstddef>
#include <cstdint>

namespace secondscreen::protocol {

// Protocol Magic: 'S', 'S', '0', '2'
inline constexpr uint8_t PROTOCOL_MAGIC[4] = {'S', 'S', '0', '2'};
inline constexpr uint32_t PROTOCOL_MAGIC_U32 = 0x53533032U;

// Current Protocol Version
inline constexpr uint16_t PROTOCOL_VERSION_2 = 2U;

// Fixed header size in bytes
inline constexpr size_t HEADER_SIZE = 24U;

// Maximum allowed payload size (16 MB)
inline constexpr uint32_t MAX_PAYLOAD_SIZE = 16U * 1024U * 1024U;

// Message/Packet Type Enumeration
enum class PacketType : uint16_t {
    Config = 0x0001U,          // Codec configuration (SPS + PPS in Annex B)
    Keyframe = 0x0002U,        // H.264 IDR access unit
    Delta = 0x0003U,           // H.264 P-slice / non-IDR access unit
    Heartbeat = 0x0004U,       // Keepalive / latency probe
    HeartbeatAck = 0x0005U,    // Heartbeat echo response
    KeyframeRequest = 0x0006U, // Client requests immediate IDR keyframe
    Shutdown = 0x0007U,        // Graceful shutdown notification
    ReceiverReport = 0x0008U,  // Client telemetry feedback
    PairRequest = 0x0009U,     // Client submits pairing token and client info
    PairResponse = 0x000AU,    // Host responds with pairing status code
    PointerEvent = 0x000BU,    // Upstream pointer movement, clicks, dragging
    KeyboardEvent = 0x000CU,   // Upstream key down/up, unicode characters
    ScrollEvent = 0x000DU      // Upstream vertical / horizontal wheel scroll
};

// Pointer Event Types
enum class PointerEventType : uint16_t {
    Move = 0x0001U,
    ButtonDown = 0x0002U,
    ButtonUp = 0x0003U,
    DoubleClick = 0x0004U
};

// Pointer Buttons
enum class PointerButton : uint8_t {
    None = 0x00U,
    Left = 0x01U,
    Right = 0x02U,
    Middle = 0x03U
};

// Keyboard Event Types
enum class KeyEventType : uint16_t {
    KeyDown = 0x0001U,
    KeyUp = 0x0002U,
    UnicodeChar = 0x0003U
};

// Modifier Flags (Bitmask)
namespace ModifierFlags {
    inline constexpr uint8_t None = 0x00U;
    inline constexpr uint8_t Shift = 0x01U;
    inline constexpr uint8_t Ctrl = 0x02U;
    inline constexpr uint8_t Alt = 0x04U;
    inline constexpr uint8_t Meta = 0x08U;
}

// Parse Status Results
enum class ParseResult {
    Success,
    NeedMoreData,
    InvalidMagic,
    UnsupportedVersion,
    UnknownPacketType,
    PayloadTooLarge,
    ZeroLengthVideoPayload
};

[[nodiscard]] constexpr const char* ParseResultToString(ParseResult result) noexcept {
    switch (result) {
        case ParseResult::Success: return "Success";
        case ParseResult::NeedMoreData: return "NeedMoreData";
        case ParseResult::InvalidMagic: return "InvalidMagic";
        case ParseResult::UnsupportedVersion: return "UnsupportedVersion";
        case ParseResult::UnknownPacketType: return "UnknownPacketType";
        case ParseResult::PayloadTooLarge: return "PayloadTooLarge";
        case ParseResult::ZeroLengthVideoPayload: return "ZeroLengthVideoPayload";
    }
    return "Unknown";
}

[[nodiscard]] constexpr const char* PacketTypeToString(PacketType type) noexcept {
    switch (type) {
        case PacketType::Config: return "Config(SPS/PPS)";
        case PacketType::Keyframe: return "Keyframe(IDR)";
        case PacketType::Delta: return "Delta(P-frame)";
        case PacketType::Heartbeat: return "Heartbeat";
        case PacketType::HeartbeatAck: return "HeartbeatAck";
        case PacketType::KeyframeRequest: return "KeyframeRequest";
        case PacketType::Shutdown: return "Shutdown";
        case PacketType::ReceiverReport: return "ReceiverReport";
        case PacketType::PairRequest: return "PairRequest";
        case PacketType::PairResponse: return "PairResponse";
        case PacketType::PointerEvent: return "PointerEvent";
        case PacketType::KeyboardEvent: return "KeyboardEvent";
        case PacketType::ScrollEvent: return "ScrollEvent";
    }
    return "UnknownType";
}

[[nodiscard]] constexpr bool IsVideoPacket(PacketType type) noexcept {
    return type == PacketType::Config ||
           type == PacketType::Keyframe ||
           type == PacketType::Delta;
}

[[nodiscard]] constexpr bool IsValidPacketType(uint16_t rawType) noexcept {
    return rawType >= static_cast<uint16_t>(PacketType::Config) &&
           rawType <= static_cast<uint16_t>(PacketType::ScrollEvent);
}

} // namespace secondscreen::protocol
