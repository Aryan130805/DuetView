#pragma once

#include "transport/transport_interface.hpp"
#include "protocol/packet_parser.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace secondscreen::transport {

class TcpTransportServer : public ITransportServer {
public:
    TcpTransportServer();
    ~TcpTransportServer() override;

    TcpTransportServer(const TcpTransportServer&) = delete;
    TcpTransportServer& operator=(const TcpTransportServer&) = delete;

    bool Start(uint16_t port, const MessageCallback& onMessage) override;
    void SetClientConnectedCallback(const ClientConnectedCallback& onConnected) override;
    void SetClientDisconnectedCallback(const ClientDisconnectedCallback& onDisconnected) override;
    bool SendPacket(const protocol::ProtocolHeader& header, std::span<const uint8_t> payload) override;
    void Stop() override;
    [[nodiscard]] bool IsClientConnected() const noexcept override;
    [[nodiscard]] std::string GetClientIp() const;

private:
    void AcceptLoop();
    void ReceiveLoop(SOCKET clientSock);
    bool SendAll(SOCKET sock, const uint8_t* data, size_t length);

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isClientConnected_{false};
    std::string clientIp_{};
    SOCKET listenSocket_{INVALID_SOCKET};
    SOCKET clientSocket_{INVALID_SOCKET};

    MessageCallback messageCallback_;
    ClientConnectedCallback clientConnectedCallback_;
    ClientDisconnectedCallback clientDisconnectedCallback_;

    std::thread acceptThread_;
    std::thread receiveThread_;
    std::mutex sendMutex_;
    std::mutex clientMutex_;

    protocol::PacketParser parser_;
    bool wsaInitialized_{false};
};

} // namespace secondscreen::transport

