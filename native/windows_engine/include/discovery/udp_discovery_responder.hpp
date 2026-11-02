#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <winsock2.h>

namespace secondscreen::discovery {

class UdpDiscoveryResponder {
public:
    UdpDiscoveryResponder();
    ~UdpDiscoveryResponder();

    bool Start(uint16_t discoveryPort, uint16_t tcpStreamPort);
    void Stop();

private:
    void ListenerLoop();
    void BeaconLoop();

    std::atomic<bool> isRunning_{false};
    SOCKET socket_{INVALID_SOCKET};
    uint16_t discoveryPort_{52026};
    uint16_t tcpStreamPort_{52025};
    std::string hostName_{"Windows PC"};

    std::thread listenerThread_{};
    std::thread beaconThread_{};
};

} // namespace secondscreen::discovery

