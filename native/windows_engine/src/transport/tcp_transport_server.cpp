#include "transport/tcp_transport_server.hpp"

#include <iostream>

namespace secondscreen::transport {

TcpTransportServer::TcpTransportServer() {
    WSADATA wsaData;
    const int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res == 0) {
        wsaInitialized_ = true;
    } else {
        std::cerr << "[ERROR] WSAStartup failed: " << res << "\n";
    }
}

TcpTransportServer::~TcpTransportServer() {
    Stop();
    if (wsaInitialized_) {
        WSACleanup();
        wsaInitialized_ = false;
    }
}

bool TcpTransportServer::Start(uint16_t port, const MessageCallback& onMessage) {
    if (!wsaInitialized_) {
        std::cerr << "[ERROR] Cannot start server: Winsock2 not initialized.\n";
        return false;
    }

    if (isRunning_.load()) {
        Stop();
    }

    messageCallback_ = onMessage;

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        std::cerr << "[ERROR] Failed to create TCP socket: " << WSAGetLastError() << "\n";
        return false;
    }

    // Allow reuse of local addresses
    BOOL opt = TRUE;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Failed to bind TCP socket to port " << port << ": " << WSAGetLastError() << "\n";
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listenSocket_, 1) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Failed to listen on TCP socket: " << WSAGetLastError() << "\n";
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    isRunning_.store(true);
    acceptThread_ = std::thread(&TcpTransportServer::AcceptLoop, this);

    std::cout << "[INFO] TCP Transport Server listening on port " << port << "...\n";
    return true;
}

void TcpTransportServer::SetClientConnectedCallback(const ClientConnectedCallback& onConnected) {
    clientConnectedCallback_ = onConnected;
}

void TcpTransportServer::SetClientDisconnectedCallback(const ClientDisconnectedCallback& onDisconnected) {
    clientDisconnectedCallback_ = onDisconnected;
}

void TcpTransportServer::AcceptLoop() {
    while (isRunning_.load()) {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);

        SOCKET newClient = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (newClient == INVALID_SOCKET) {
            if (!isRunning_.load()) {
                break;
            }
            continue;
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddr.sin_port);
        std::cout << "[INFO] Android client connected from " << ipStr << ":" << clientPort << "\n";

        // Enable TCP_NODELAY for lowest latency streaming
        BOOL noDelay = TRUE;
        setsockopt(newClient, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

        // Enlarge send and receive buffers to prevent frame truncation and buffer exhaustion on keyframes
        int sndBuf = 1024 * 1024; // 1 MB
        setsockopt(newClient, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndBuf), sizeof(sndBuf));
        int rcvBuf = 512 * 1024;  // 512 KB
        setsockopt(newClient, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBuf), sizeof(rcvBuf));

        // Close any prior client socket
        {
            std::lock_guard<std::mutex> lock(clientMutex_);
            if (clientSocket_ != INVALID_SOCKET) {
                closesocket(clientSocket_);
            }
            clientSocket_ = newClient;
            clientIp_ = ipStr;
            isClientConnected_.store(true);
        }

        parser_.Reset();

        // Notify engine that a fresh client has connected
        if (clientConnectedCallback_) {
            clientConnectedCallback_();
        }

        // Wait for prior receive thread to join if needed
        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }

        receiveThread_ = std::thread(&TcpTransportServer::ReceiveLoop, this, newClient);
    }
}

void TcpTransportServer::ReceiveLoop(SOCKET clientSock) {
    std::vector<uint8_t> recvBuf(64 * 1024);

    while (isRunning_.load() && isClientConnected_.load()) {
        const int bytesRead = recv(clientSock, reinterpret_cast<char*>(recvBuf.data()), static_cast<int>(recvBuf.size()), 0);
        if (bytesRead <= 0) {
            std::cout << "[INFO] Client disconnected or socket closed.\n";
            break;
        }

        const std::span<const uint8_t> chunk(recvBuf.data(), static_cast<size_t>(bytesRead));

        const auto onPacket = [this](const protocol::ProtocolHeader& header, std::span<const uint8_t> payload) {
            if (messageCallback_) {
                messageCallback_(header, payload);
            }
        };

        const auto onError = [](protocol::ParseResult res, const std::string& msg) {
            std::cerr << "[WARNING] Protocol parse error: " << msg << " (" << protocol::ParseResultToString(res) << ")\n";
        };

        parser_.Consume(chunk, onPacket, onError);
    }

    {
        std::lock_guard<std::mutex> lock(clientMutex_);
        if (clientSocket_ == clientSock) {
            closesocket(clientSocket_);
            clientSocket_ = INVALID_SOCKET;
            clientIp_.clear();
            isClientConnected_.store(false);
        }
    }

    if (clientDisconnectedCallback_) {
        clientDisconnectedCallback_();
    }
}

bool TcpTransportServer::SendPacket(const protocol::ProtocolHeader& header, std::span<const uint8_t> payload) {
    if (!isClientConnected_.load()) {
        return false;
    }

    const auto headerBytes = header.ToBytes();

    std::lock_guard<std::mutex> lock(sendMutex_);

    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> cLock(clientMutex_);
        sock = clientSocket_;
    }

    if (sock == INVALID_SOCKET) {
        return false;
    }

    // 1. Send 24-byte header
    if (!SendAll(sock, headerBytes.data(), headerBytes.size())) {
        return false;
    }

    // 2. Send payload if present
    if (!payload.empty()) {
        if (!SendAll(sock, payload.data(), payload.size())) {
            return false;
        }
    }

    return true;
}

bool TcpTransportServer::SendAll(SOCKET sock, const uint8_t* data, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        const int sent = send(sock, reinterpret_cast<const char*>(data + totalSent), static_cast<int>(length - totalSent), 0);
        if (sent == SOCKET_ERROR || sent <= 0) {
            const int err = WSAGetLastError();
            if (err == WSAEINTR) {
                continue;
            }
            std::cerr << "[WARNING] Socket send failed with error: " << err << "\n";
            isClientConnected_.store(false);
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

void TcpTransportServer::Stop() {
    isRunning_.store(false);
    isClientConnected_.store(false);

    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(clientMutex_);
        if (clientSocket_ != INVALID_SOCKET) {
            closesocket(clientSocket_);
            clientSocket_ = INVALID_SOCKET;
        }
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

bool TcpTransportServer::IsClientConnected() const noexcept {
    return isClientConnected_.load();
}

std::string TcpTransportServer::GetClientIp() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(clientMutex_));
    return isClientConnected_.load() ? clientIp_ : "";
}

} // namespace secondscreen::transport

