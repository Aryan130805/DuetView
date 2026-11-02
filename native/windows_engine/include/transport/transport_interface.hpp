#pragma once

#include "protocol/protocol_constants.hpp"
#include "protocol/protocol_header.hpp"

#include <cstdint>
#include <functional>
#include <span>

namespace secondscreen::transport {

using MessageCallback = std::function<void(const protocol::ProtocolHeader& header, std::span<const uint8_t> payload)>;
using ClientConnectedCallback = std::function<void()>;
using ClientDisconnectedCallback = std::function<void()>;

class ITransportServer {
public:
    virtual ~ITransportServer() = default;

    /// Starts listening on specified TCP port.
    virtual bool Start(uint16_t port, const MessageCallback& onMessage) = 0;

    /// Registers a callback triggered when an Android client connects.
    virtual void SetClientConnectedCallback(const ClientConnectedCallback& onConnected) = 0;

    /// Registers a callback triggered when an Android client disconnects.
    virtual void SetClientDisconnectedCallback(const ClientDisconnectedCallback& onDisconnected) = 0;

    /// Broadcasts or sends packet to connected client.
    virtual bool SendPacket(const protocol::ProtocolHeader& header, std::span<const uint8_t> payload) = 0;

    /// Stops listening and disconnects active clients.
    virtual void Stop() = 0;

    /// Returns true if an active client is connected.
    [[nodiscard]] virtual bool IsClientConnected() const noexcept = 0;
};

} // namespace secondscreen::transport
