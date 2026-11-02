#include "pairing/pairing_manager.hpp"
#include "pairing/qrcodegen.hpp"

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace secondscreen::pairing {

std::string QrPayload::ToJson() const {
    std::ostringstream ss;
    ss << "{\"v\":" << version
       << ",\"id\":\"" << id << "\""
       << ",\"name\":\"" << name << "\""
       << ",\"endpoints\":[";
    for (size_t i = 0; i < endpoints.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << endpoints[i] << "\"";
    }
    ss << "],\"port\":" << port
       << ",\"pair_port\":" << pairPort
       << ",\"token\":\"" << token << "\""
       << ",\"exp\":" << expirySec
       << "}";
    return ss.str();
}

bool QrPayload::Parse(const std::string& json, QrPayload& outPayload) {
    if (json.empty() || json.front() != '{' || json.back() != '}') {
        return false;
    }

    const auto extractString = [&](const std::string& key) -> std::string {
        const std::string pattern = "\"" + key + "\":\"";
        const size_t pos = json.find(pattern);
        if (pos == std::string::npos) return "";
        const size_t start = pos + pattern.size();
        const size_t end = json.find("\"", start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    };

    const auto extractInt = [&](const std::string& key) -> uint64_t {
        const std::string pattern = "\"" + key + "\":";
        const size_t pos = json.find(pattern);
        if (pos == std::string::npos) return 0;
        const size_t start = pos + pattern.size();
        size_t end = json.find_first_of(",}]", start);
        if (end == std::string::npos) end = json.size();
        try {
            return std::stoull(json.substr(start, end - start));
        } catch (...) {
            return 0;
        }
    };

    outPayload.version = static_cast<uint32_t>(extractInt("v"));
    outPayload.id = extractString("id");
    outPayload.name = extractString("name");
    outPayload.port = static_cast<uint16_t>(extractInt("port"));
    const uint64_t pp = extractInt("pair_port");
    outPayload.pairPort = (pp > 0) ? static_cast<uint16_t>(pp) : outPayload.port;
    outPayload.token = extractString("token");
    outPayload.expirySec = extractInt("exp");

    // Extract endpoints array
    outPayload.endpoints.clear();
    const size_t arrPos = json.find("\"endpoints\":[");
    if (arrPos != std::string::npos) {
        const size_t arrStart = arrPos + 13;
        const size_t arrEnd = json.find("]", arrStart);
        if (arrEnd != std::string::npos) {
            std::string items = json.substr(arrStart, arrEnd - arrStart);
            size_t p = 0;
            while (p < items.size()) {
                size_t q1 = items.find("\"", p);
                if (q1 == std::string::npos) break;
                size_t q2 = items.find("\"", q1 + 1);
                if (q2 == std::string::npos) break;
                outPayload.endpoints.push_back(items.substr(q1 + 1, q2 - q1 - 1));
                p = q2 + 1;
            }
        }
    }

    return (outPayload.version >= 1U && !outPayload.id.empty() && !outPayload.token.empty() && outPayload.port > 0U);
}

PairingManager::PairingManager() = default;

PairingManager::~PairingManager() {
    StopPairingServer();
}

bool PairingManager::Initialize(const std::string& customName, uint16_t port, uint16_t pairPort) {
    port_ = port;
    pairPort_ = pairPort;
    pcId_ = LoadOrCreateStablePcId();

    if (!customName.empty()) {
        pcName_ = customName;
    } else {
        char buf[MAX_COMPUTERNAME_LENGTH + 1] = {0};
        DWORD sz = sizeof(buf);
        if (GetComputerNameA(buf, &sz)) {
            pcName_ = buf;
        } else {
            pcName_ = "Windows PC";
        }
    }

    GenerateNewPairingToken();
    return true;
}

bool PairingManager::StartPairingServer(uint16_t pairPort) {
    StopPairingServer();
    pairPort_ = pairPort;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return false;
    }

    BOOL opt = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(pairPort_);

    if (bind(s, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }

    if (listen(s, 5) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }

    serverSocket_ = static_cast<uintptr_t>(s);
    isServerRunning_ = true;
    serverThread_ = std::make_unique<std::thread>(&PairingManager::PairingServerLoop, this);
    return true;
}

void PairingManager::StopPairingServer() {
    isServerRunning_ = false;
    if (serverSocket_ != ~static_cast<uintptr_t>(0)) {
        closesocket(static_cast<SOCKET>(serverSocket_));
        serverSocket_ = ~static_cast<uintptr_t>(0);
    }
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
    }
    serverThread_.reset();
}

bool PairingManager::IsPairingServerRunning() const noexcept {
    return isServerRunning_.load();
}

void PairingManager::PairingServerLoop() {
    SOCKET listenSock = static_cast<SOCKET>(serverSocket_);

    while (isServerRunning_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSock, &readSet);

        timeval tv{0, 250000}; // 250ms timeout
        int res = select(0, &readSet, nullptr, nullptr, &tv);
        if (res <= 0 || !isServerRunning_) {
            continue;
        }

        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSock == INVALID_SOCKET) {
            continue;
        }

        // Set 2s timeout on client socket
        DWORD timeoutMs = 2000;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
        setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

        std::vector<uint8_t> buffer(4096);
        int bytesRead = recv(clientSock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
        if (bytesRead > 0) {
            std::string req(reinterpret_cast<const char*>(buffer.data()), bytesRead);
            std::string token;
            size_t tokenPos = req.find("\"token\":\"");
            if (tokenPos != std::string::npos) {
                size_t start = tokenPos + 9;
                size_t end = req.find("\"", start);
                if (end != std::string::npos) {
                    token = req.substr(start, end - start);
                }
            }

            const auto status = ValidatePairingToken(token);
            std::string respJson = "{\"status\":" + std::to_string(static_cast<uint32_t>(status)) +
                                   ",\"name\":\"" + pcName_ +
                                   "\",\"id\":\"" + pcId_ + "\"}";

            // Prepare SS02 packet header:
            // Magic: 'S','S','0','2' (0x53, 0x53, 0x30, 0x32)
            // Version: 0x0002
            // PacketType: 0x000A (PairResponse)
            // Sequence: 1
            // PayloadLength: len
            // TimestampUs: now
            std::vector<uint8_t> packet(24 + respJson.size());
            packet[0] = 'S'; packet[1] = 'S'; packet[2] = '0'; packet[3] = '2';
            packet[4] = 0x00; packet[5] = 0x02; // version 2
            packet[6] = 0x00; packet[7] = 0x0A; // PairResponse (0x000A)
            packet[8] = 0; packet[9] = 0; packet[10] = 0; packet[11] = 1; // seq 1
            const uint32_t payloadLen = static_cast<uint32_t>(respJson.size());
            packet[12] = static_cast<uint8_t>((payloadLen >> 24) & 0xFF);
            packet[13] = static_cast<uint8_t>((payloadLen >> 16) & 0xFF);
            packet[14] = static_cast<uint8_t>((payloadLen >> 8) & 0xFF);
            packet[15] = static_cast<uint8_t>(payloadLen & 0xFF);

            const uint64_t nowUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            for (int i = 0; i < 8; ++i) {
                packet[16 + i] = static_cast<uint8_t>((nowUs >> (56 - i * 8)) & 0xFF);
            }

            memcpy(packet.data() + 24, respJson.data(), respJson.size());
            send(clientSock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
            shutdown(clientSock, SD_SEND);
            char dummy[32];
            recv(clientSock, dummy, sizeof(dummy), 0);
        }

        closesocket(clientSock);
    }
}

std::string PairingManager::LoadOrCreateStablePcId() {
    char appData[MAX_PATH];
    std::string idFilePath = "secondscreen_pc_id.txt";

    if (GetEnvironmentVariableA("LOCALAPPDATA", appData, MAX_PATH) > 0) {
        std::string dir = std::string(appData) + "\\SecondScreen";
        CreateDirectoryA(dir.c_str(), nullptr);
        idFilePath = dir + "\\pc_id.txt";
    }

    // Try reading existing ID
    std::ifstream inFile(idFilePath);
    if (inFile.is_open()) {
        std::string existingId;
        if (std::getline(inFile, existingId) && !existingId.empty()) {
            return existingId;
        }
    }

    // Generate stable UUID: pc-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    std::string newId = "pc-" + GenerateRandomHex(4) + "-" +
                        GenerateRandomHex(2) + "-" +
                        GenerateRandomHex(2) + "-" +
                        GenerateRandomHex(2) + "-" +
                        GenerateRandomHex(6);

    std::ofstream outFile(idFilePath);
    if (outFile.is_open()) {
        outFile << newId;
    }

    return newId;
}

std::string PairingManager::GenerateRandomHex(size_t byteCount) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 255);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < byteCount; ++i) {
        ss << std::setw(2) << dist(gen);
    }
    return ss.str();
}

void PairingManager::GenerateNewPairingToken(uint32_t ttlSeconds) {
    currentToken_ = GenerateRandomHex(16); // 32-char hex token
    tokenExpiry_ = std::chrono::system_clock::now() + std::chrono::seconds(ttlSeconds);
}

void PairingManager::RevokePairingToken() {
    currentToken_.clear();
    tokenExpiry_ = std::chrono::system_clock::time_point{};
}

bool PairingManager::HasActiveToken() const {
    if (currentToken_.empty()) return false;
    return std::chrono::system_clock::now() < tokenExpiry_;
}

PairAuthStatus PairingManager::ValidatePairingToken(const std::string& token) const {
    if (token.empty() || currentToken_.empty()) {
        return PairAuthStatus::InvalidToken;
    }
    if (std::chrono::system_clock::now() > tokenExpiry_) {
        return PairAuthStatus::ExpiredToken;
    }
    if (token == currentToken_) {
        return PairAuthStatus::Success;
    }
    return PairAuthStatus::InvalidToken;
}

std::vector<std::string> PairingManager::GetLocalIPv4Endpoints() const {
    std::vector<std::string> wifiEndpoints;
    std::vector<std::string> lanEndpoints;
    std::vector<std::string> otherEndpoints;

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

            // Check adapter name / description for virtual adapters
            std::wstring desc = pCurr->Description ? pCurr->Description : L"";
            std::wstring friendly = pCurr->FriendlyName ? pCurr->FriendlyName : L"";
            std::wstring combined = desc + L" " + friendly;
            for (auto& ch : combined) ch = static_cast<wchar_t>(towlower(ch));

            const bool isVirtual = (combined.find(L"hyper-v") != std::wstring::npos ||
                                    combined.find(L"vethernet") != std::wstring::npos ||
                                    combined.find(L"virtual") != std::wstring::npos ||
                                    combined.find(L"vmware") != std::wstring::npos ||
                                    combined.find(L"wsl") != std::wstring::npos ||
                                    combined.find(L"vpn") != std::wstring::npos ||
                                    combined.find(L"tap") != std::wstring::npos);

            for (PIP_ADAPTER_UNICAST_ADDRESS pUni = pCurr->FirstUnicastAddress; pUni != nullptr; pUni = pUni->Next) {
                sockaddr* sa = pUni->Address.lpSockaddr;
                if (sa && sa->sa_family == AF_INET) {
                    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(sa);
                    char ipStr[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &(sin->sin_addr), ipStr, sizeof(ipStr))) {
                        std::string ip(ipStr);
                        // Filter out loopback and APIPA auto-config (169.254.*)
                        if (!ip.empty() && ip != "127.0.0.1" && ip.rfind("169.254.", 0) != 0) {
                            if (pCurr->IfType == IF_TYPE_IEEE80211) {
                                wifiEndpoints.push_back(ip);
                            } else if (!isVirtual && pCurr->IfType == IF_TYPE_ETHERNET_CSMACD) {
                                lanEndpoints.push_back(ip);
                            } else {
                                otherEndpoints.push_back(ip);
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<std::string> endpoints;
    // Wi-Fi is top priority for wireless companion display
    endpoints.insert(endpoints.end(), wifiEndpoints.begin(), wifiEndpoints.end());
    // Physical Ethernet LAN is second priority
    endpoints.insert(endpoints.end(), lanEndpoints.begin(), lanEndpoints.end());
    // Fallbacks
    endpoints.insert(endpoints.end(), otherEndpoints.begin(), otherEndpoints.end());

    if (endpoints.empty()) {
        endpoints.push_back("127.0.0.1");
    }

    return endpoints;
}

QrPayload PairingManager::GetCurrentPayload() const {
    QrPayload payload{};
    payload.version = 1U;
    payload.id = pcId_;
    payload.name = pcName_;
    payload.endpoints = GetLocalIPv4Endpoints();
    payload.port = port_;
    payload.pairPort = pairPort_;
    payload.token = currentToken_;

    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
        tokenExpiry_.time_since_epoch()).count();
    payload.expirySec = static_cast<uint64_t>(std::max<int64_t>(0, sec));

    return payload;
}

void PairingManager::PrintConsoleQrCode() const {
    const auto payload = GetCurrentPayload();
    const std::string json = payload.ToJson();

    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(json.c_str(), qrcodegen::QrCode::Ecc::LOW);
    const int size = qr.getSize();
    const int border = 2;

    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "                    SecondScreen QR Pairing                           \n";
    std::cout << "======================================================================\n";
    std::cout << " PC Name  : " << payload.name << "\n";
    std::cout << " PC ID    : " << payload.id << "\n";
    std::cout << " Port     : " << payload.port << "\n";
    std::cout << " Local IPs: ";
    for (size_t i = 0; i < payload.endpoints.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << payload.endpoints[i];
    }
    std::cout << "\n";
    std::cout << " Token TTL: ~5 minutes (regenerates automatically)\n";
    std::cout << " Instructions: Scan with Android SecondScreen 'Add PC' button.\n";
    std::cout << "----------------------------------------------------------------------\n\n";

    // White quiet border top
    for (int y = -border; y < 0; ++y) {
        for (int x = -border; x < size + border; ++x) {
            std::cout << "\xE2\x96\x88\xE2\x96\x88"; // Full white block in dark terminal
        }
        std::cout << "\n";
    }

    // QR Body (using standard block elements)
    for (int y = 0; y < size; ++y) {
        for (int b = 0; b < border; ++b) std::cout << "\xE2\x96\x88\xE2\x96\x88";
        for (int x = 0; x < size; ++x) {
            if (qr.getModule(x, y)) {
                std::cout << "  "; // Black module
            } else {
                std::cout << "\xE2\x96\x88\xE2\x96\x88"; // White module
            }
        }
        for (int b = 0; b < border; ++b) std::cout << "\xE2\x96\x88\xE2\x96\x88";
        std::cout << "\n";
    }

    // White quiet border bottom
    for (int y = 0; y < border; ++y) {
        for (int x = -border; x < size + border; ++x) {
            std::cout << "\xE2\x96\x88\xE2\x96\x88";
        }
        std::cout << "\n";
    }

    std::cout << "\n======================================================================\n";
    std::cout << " Pairing Payload JSON:\n " << json << "\n";
    std::cout << "======================================================================\n\n";
}

} // namespace secondscreen::pairing

