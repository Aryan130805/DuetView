#include "discovery/udp_discovery_responder.hpp"

#include <iostream>
#include <ws2tcpip.h>
#include <windows.h>

namespace secondscreen::discovery {

UdpDiscoveryResponder::UdpDiscoveryResponder() {
    char nameBuf[256] = {0};
    DWORD len = sizeof(nameBuf);
    if (GetComputerNameA(nameBuf, &len)) {
        hostName_ = nameBuf;
    }
}

UdpDiscoveryResponder::~UdpDiscoveryResponder() {
    Stop();
}

bool UdpDiscoveryResponder::Start(uint16_t discoveryPort, uint16_t tcpStreamPort) {
    if (isRunning_.load()) {
        Stop();
    }

    discoveryPort_ = discoveryPort;
    tcpStreamPort_ = tcpStreamPort;

    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        std::cerr << "[ERROR] Failed to create UDP discovery socket: " << WSAGetLastError() << "\n";
        return false;
    }

    BOOL broadcastOpt = TRUE;
    setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcastOpt), sizeof(broadcastOpt));

    BOOL reuseOpt = TRUE;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseOpt), sizeof(reuseOpt));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = htons(discoveryPort);

    if (bind(socket_, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Failed to bind UDP discovery socket to port " << discoveryPort
                  << ": " << WSAGetLastError() << "\n";
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }

    isRunning_.store(true);
    listenerThread_ = std::thread(&UdpDiscoveryResponder::ListenerLoop, this);
    beaconThread_ = std::thread(&UdpDiscoveryResponder::BeaconLoop, this);

    std::cout << "[INFO] UDP Discovery Responder active on port " << discoveryPort << ".\n";
    return true;
}

void UdpDiscoveryResponder::Stop() {
    if (!isRunning_.exchange(false)) {
        return;
    }

    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    if (listenerThread_.joinable()) {
        listenerThread_.join();
    }
    if (beaconThread_.joinable()) {
        beaconThread_.join();
    }
}

void UdpDiscoveryResponder::ListenerLoop() {
    char recvBuf[1024];

    while (isRunning_.load()) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);

        const int bytes = recvfrom(socket_, recvBuf, sizeof(recvBuf) - 1, 0,
                                   reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (bytes <= 0) {
            if (!isRunning_.load()) break;
            continue;
        }

        recvBuf[bytes] = '\0';
        std::string request(recvBuf);

        if (request.find("SECONDSCREEN_DISCOVER") != std::string::npos) {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);

            std::string reply = "SECONDSCREEN_BEACON:{\"name\":\"" + hostName_ +
                                "\",\"port\":" + std::to_string(tcpStreamPort_) +
                                ",\"osName\":\"Windows\"}";

            sendto(socket_, reply.c_str(), static_cast<int>(reply.size()), 0,
                   reinterpret_cast<sockaddr*>(&clientAddr), clientLen);

            std::cout << "[INFO] Replied to UDP discovery probe from " << ipStr << "\n";
        }
    }
}

void UdpDiscoveryResponder::BeaconLoop() {
    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_port = htons(discoveryPort_);

    std::string beacon = "SECONDSCREEN_BEACON:{\"name\":\"" + hostName_ +
                         "\",\"port\":" + std::to_string(tcpStreamPort_) +
                         ",\"osName\":\"Windows\"}";

    while (isRunning_.load()) {
        if (socket_ != INVALID_SOCKET) {
            sendto(socket_, beacon.c_str(), static_cast<int>(beacon.size()), 0,
                   reinterpret_cast<sockaddr*>(&broadcastAddr), sizeof(broadcastAddr));
        }

        // Sleep for 2.5 seconds between periodic beacons
        for (int i = 0; i < 25 && isRunning_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace secondscreen::discovery

