#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>

namespace secondscreen::pairing {

enum class PairAuthStatus : uint32_t {
    Success = 0U,
    InvalidToken = 1U,
    ExpiredToken = 2U,
    MalformedRequest = 3U
};

enum class InterfaceKind : uint32_t {
    Wifi = 0U,
    UsbTethered = 1U,
    EthernetLan = 2U,
    Other = 3U
};

struct NetworkEndpointInfo {
    std::string ipAddress;
    std::string subnetMask;
    std::string broadcastAddress;
    std::string gateway;
    std::string adapterName;
    std::string adapterDescription;
    InterfaceKind kind{InterfaceKind::Other};
    uint32_t ifType{0};
    uint32_t ifIndex{0};
    bool isUsbTethered{false};
};

struct QrPayload {
    uint32_t version{1U};
    std::string id;
    std::string name;
    std::vector<std::string> endpoints;
    uint16_t port{52025U};
    uint16_t pairPort{52027U};
    std::string token;
    uint64_t expirySec{0U};

    [[nodiscard]] std::string ToJson() const;
    static bool Parse(const std::string& json, QrPayload& outPayload);
};

class PairingManager {
public:
    PairingManager();
    ~PairingManager();

    /// Initializes stable PC ID and display name
    bool Initialize(const std::string& customName = "", uint16_t port = 52025U, uint16_t pairPort = 52027U);

    /// Starts a dedicated background TCP pairing listener (default port 52027)
    bool StartPairingServer(uint16_t pairPort = 52027U);

    /// Stops the background pairing listener
    void StopPairingServer();

    [[nodiscard]] bool IsPairingServerRunning() const noexcept;

    /// Generates a new short-lived pairing token (default 300s = 5 min)
    void GenerateNewPairingToken(uint32_t ttlSeconds = 300U);

    /// Revokes current token (e.g. after pairing or on manual reset)
    void RevokePairingToken();

    /// Validates an incoming pairing token from PACKET_TYPE_PAIR_REQ
    [[nodiscard]] PairAuthStatus ValidatePairingToken(const std::string& token) const;

    /// Returns all active, reachable non-loopback IPv4 endpoints on the host PC
    [[nodiscard]] std::vector<std::string> GetLocalIPv4Endpoints() const;

    /// Returns detailed network interface information including USB tethering status
    [[nodiscard]] std::vector<NetworkEndpointInfo> GetAllEndpointsDetailed() const;

    /// Returns the active USB-tethered IPv4 address if detected, empty string otherwise
    [[nodiscard]] std::string GetUsbTetheredEndpoint() const;

    /// Returns the primary active Wi-Fi IPv4 address if detected, empty string otherwise
    [[nodiscard]] std::string GetWifiEndpoint() const;

    /// Generates and returns current active QR JSON payload
    [[nodiscard]] QrPayload GetCurrentPayload() const;

    /// Prints a high-contrast ANSI UTF-8 QR code directly to the console
    void PrintConsoleQrCode() const;

    [[nodiscard]] std::string GetPcId() const noexcept { return pcId_; }
    [[nodiscard]] std::string GetPcName() const noexcept { return pcName_; }
    [[nodiscard]] uint16_t GetPort() const noexcept { return port_; }
    [[nodiscard]] uint16_t GetPairPort() const noexcept { return pairPort_; }
    [[nodiscard]] bool HasActiveToken() const;

private:
    std::string LoadOrCreateStablePcId();
    static std::string GenerateRandomHex(size_t byteCount);
    void PairingServerLoop();

    std::string pcId_;
    std::string pcName_;
    uint16_t port_{52025U};
    uint16_t pairPort_{52027U};

    std::string currentToken_;
    std::chrono::system_clock::time_point tokenExpiry_{};

    // Dedicated pairing listener members
    uintptr_t serverSocket_{~static_cast<uintptr_t>(0)};
    std::unique_ptr<std::thread> serverThread_;
    std::atomic<bool> isServerRunning_{false};
};

} // namespace secondscreen::pairing

