#include "discovery/udp_discovery_responder.hpp"

#include <iostream>
#include <vector>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

namespace secondscreen::discovery {

static std::vector<uint32_t> GetSubnetBroadcastAddresses() {
    std::vector<uint32_t> broadcasts;
    ULONG outBufLen = 15000;
    std::vector<uint8_t> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    DWORD ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    }

    if (ret == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next) {
            if (pCurr->OperStatus != IfOperStatusUp) continue;
            if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            for (PIP_ADAPTER_UNICAST_ADDRESS pUni = pCurr->FirstUnicastAddress; pUni != nullptr; pUni = pUni->Next) {
                sockaddr* sa = pUni->Address.lpSockaddr;
                if (sa && sa->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(sa);
                    const uint32_t ipVal = sin->sin_addr.s_addr;
                    const uint8_t prefixLen = pUni->OnLinkPrefixLength;
                    if (prefixLen > 0 && prefixLen <= 32) {
                        const uint32_t netMask = htonl(~0U << (32 - prefixLen));
                        const uint32_t bcastVal = ipVal | (~netMask);
                        if (bcastVal != 0 && bcastVal != INADDR_BROADCAST && bcastVal != ipVal) {
                            broadcasts.push_back(bcastVal);
                        }
                    }
                }
            }
        }
    }
    return broadcasts;
}

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
            // 1. Send to global broadcast 255.255.255.255
            sendto(socket_, beacon.c_str(), static_cast<int>(beacon.size()), 0,
                   reinterpret_cast<sockaddr*>(&broadcastAddr), sizeof(broadcastAddr));

            // 2. Broadcast on each active subnet directed broadcast (essential for USB Tethering & multi-homed links)
            const auto subnets = GetSubnetBroadcastAddresses();
            for (const uint32_t bcast : subnets) {
                sockaddr_in targetAddr{};
                targetAddr.sin_family = AF_INET;
                targetAddr.sin_addr.s_addr = bcast;
                targetAddr.sin_port = htons(discoveryPort_);
                sendto(socket_, beacon.c_str(), static_cast<int>(beacon.size()), 0,
                       reinterpret_cast<sockaddr*>(&targetAddr), sizeof(targetAddr));
            }
        }

        // Sleep for 2.0 seconds between periodic beacons
        for (int i = 0; i < 20 && isRunning_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace secondscreen::discovery

