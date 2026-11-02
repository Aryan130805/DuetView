# SecondScreen — QR Pairing and Saved-PC Management Architecture & Guide

This document specifies the production QR pairing mechanism, saved-PC persistence, and background health-check architecture for the SecondScreen Windows-to-Android streaming ecosystem.

---

## 1. Architectural Overview

SecondScreen provides seamless local network pairing between the native Windows host engine (`secondscreen_engine.exe`) and the Flutter/Android client (`app-debug.apk`).

```
+------------------------------------+             +---------------------------------------+
|        Windows Native Host         |             |          Android Client               |
|                                    |             |                                       |
|  1. Stable UUID (pc_id.txt)        |             |  1. QR Scanner (MobileScanner)        |
|  2. Adapter Enum (GetAdaptersAddr) |             |  2. Schema v1 Validation & Expiry     |
|  3. 5-Minute Ephemeral Token       |   QR Scan   |  3. Multi-adapter Handshake Probe     |
|  4. ANSI / HTML QR Generation      | ----------> |     (PACKET_TYPE_PAIR_REQ 0x0009)     |
|  5. SS02 TCP Listener (52025)      |             |  4. Confirm & Save to SharedPreferences|
|                                    | <---------- |  5. 5s Heartbeat Health Check         |
|  6. Pair Verification Response     |  Handshake  |     (PACKET_TYPE_HEARTBEAT 0x0004)    |
|     (PACKET_TYPE_PAIR_RESP 0x000A) |             |  6. Active Streaming Handoff          |
+------------------------------------+             +---------------------------------------+
```

---

## 2. QR Code Payload Schema (Version 1)

The pairing QR code encodes a compact JSON payload containing host identity, active endpoints, and an ephemeral authentication token.

### 2.1 Specification

```json
{
  "v": 1,
  "id": "e4b57497-21a4-44b4-82a9-c12e52467d1d",
  "name": "Aryan-PC",
  "endpoints": [
    "192.168.1.15:52025",
    "10.0.0.4:52025"
  ],
  "token": "4a7f0c1b9e2d83fa",
  "exp": 1793375400
}
```

### 2.2 Field Definitions

| Field | Type | Description |
|---|---|---|
| `v` | `integer` | Schema version (must be `1`). |
| `id` | `string` | Stable PC identifier (UUID v4), preserved in `pc_id.txt` across restarts. |
| `name` | `string` | Human-readable PC display name (customizable via `--name`). |
| `endpoints` | `array[string]` | Candidate IPv4 addresses and TCP ports (`<ip>:<port>`), ordered by adapter priority. |
| `token` | `string` | 16-character cryptographically random hex authentication token. |
| `exp` | `integer` | Unix timestamp (seconds) after which the token expires (default: 300 seconds / 5 minutes). |

---

## 3. Pairing and Authentication Protocol Flow

Pairing uses lightweight SS02 packet framing (`0x53 0x53 0x30 0x32`) without launching desktop capture or video encoding pipelines.

### 3.1 Packet Types

| Packet Type | Opcode | Direction | Description |
|---|---|---|---|
| `PACKET_TYPE_PAIR_REQ` | `0x0009` | Client $\rightarrow$ Host | Android requests pairing with token & device name. |
| `PACKET_TYPE_PAIR_RESP` | `0x000A` | Host $\rightarrow$ Client | Host verifies token and confirms pairing status. |

### 3.2 Handshake Sequence

1. **Host Generation**: Host generates pairing token valid for 5 minutes (`token`, `exp`) and displays QR code on the terminal and HTML artifact (`secondscreen_pair.html`).
2. **Client Scan**: Android camera reads QR code or user pastes JSON payload.
3. **Payload Verification**:
   - Validates JSON schema version (`v == 1`), non-empty `id`, and at least one endpoint.
   - Rejects expired tokens (`exp < currentTime`).
4. **Network Probe**:
   - Client iterates over candidate endpoints in parallel/sequential timeout probes (3-second timeout).
   - Upon TCP connection, sends `PACKET_TYPE_PAIR_REQ` containing:
     ```json
     {"token": "4a7f0c1b9e2d83fa", "client_name": "Pixel 7"}
     ```
5. **Host Validation**:
   - Host checks token validity against active pairing session.
   - If valid, host responds with `PACKET_TYPE_PAIR_RESP`:
     ```json
     {"status": "ok", "pc_id": "...", "pc_name": "Aryan-PC"}
     ```
   - If invalid or expired, host returns `status: "invalid_token"` or `status: "expired"` and closes socket.
6. **Persistence**:
   - Client confirms with user and persists PC to `SharedPreferences`.

---

## 4. Background Health-Check Protocol (Heartbeat)

To monitor availability without spinning up video pipelines or draining battery:

### 4.1 Packet Types

| Packet Type | Opcode | Direction | Description |
|---|---|---|---|
| `PACKET_TYPE_HEARTBEAT` | `0x0004` | Client $\rightarrow$ Host | 16-byte ping packet with monotonic sequence ID and timestamp. |
| `PACKET_TYPE_HEARTBEAT_ACK` | `0x0005` | Host $\rightarrow$ Client | 16-byte pong packet echoing sequence ID and host timestamp. |

### 4.2 Behavior & Battery Optimization

- **Interval**: 5 seconds.
- **Probe Timeout**: 2.5 seconds per endpoint.
- **State Transitions**:
  - 1 successful reply $\rightarrow$ Mark `online`.
  - 3 consecutive missed probes $\rightarrow$ Mark `offline`.
- **App Lifecycle Pausing**: `PcHealthService` implements `WidgetsBindingObserver`. When the Flutter app transitions to `AppLifecycleState.paused` or `detached`, all periodic polling timers are halted immediately. Timers resume on `resumed`.
- **Active Streaming Exclusion**: If a PC is currently streaming video (`isActivelyStreaming(pc.id) == true`), heartbeat checks for that PC are bypassed because the active video TCP connection already monitors connection health.

---

## 5. Saved-PC Management & Deduplication

### 5.1 Deduplication on Re-Scan

When a user scans a pairing QR code for a PC that is already saved:
1. **Match by PC ID**: The storage lookup checks `pc.id` (stable UUID).
2. **Endpoint Update**: New candidate IP addresses are merged into the existing PC entry without losing previous endpoints.
3. **Preserve User Customizations**: If the user previously renamed "Aryan-PC" to "Bedroom Workstation", the custom display name is preserved unless explicitly modified.
4. **Last-Seen Refresh**: Updates the `lastSeen` timestamp.

### 5.2 Multi-PC Support

Users can pair multiple Windows hosts (e.g., "Workstation", "Laptop", "Living Room PC").
- Each card displays host name, active/reachable IP endpoint, and a color-coded status indicator:
  - **Green (Shadowed)**: Online & reachable.
  - **Red / Grey**: Offline or unreachable.
  - **Blue**: Currently streaming.
- Inline rename dialog allows custom friendly names.
- Delete confirmation prevents accidental removal.

---

## 6. Network Configuration & Firewall Troubleshooting

### 6.1 Network Ports

| Port | Protocol | Purpose |
|---|---|---|
| `52025` | TCP | SS02 Protocol: Pairing, Heartbeat, H.264 Video Stream, Input Control. |
| `52026` | UDP | Discovery broadcasts (Phase 3 fallback). |

### 6.2 Windows Firewall Rule Setup

If Android cannot reach Windows over Wi-Fi, ensure Windows Defender Firewall allows port 52025 TCP. Run in elevated PowerShell:

```powershell
New-NetFirewallRule -DisplayName "SecondScreen Host Stream" -Direction Inbound -Protocol TCP -LocalPort 52025 -Action Allow
```

### 6.3 Multiple Network Adapters (VirtualBox, Hyper-V, VPNs)

Windows machines often have virtual network adapters (Docker, WSL, VirtualBox, ZeroTier).
- The Windows engine queries all non-loopback, operational IPv4 adapters using `GetAdaptersAddresses`.
- All viable LAN IPs are included in `endpoints` array of the QR code.
- Android attempts connections across these endpoints in order, ensuring connectivity even if virtual adapters are listed.

---

## 7. Manual Connection Fallback

If camera permissions are denied or the camera cannot focus:
1. Tap the QR scanner screen's **"Enter Code Manually"** button or use the connection screen.
2. Paste the JSON QR payload string directly into the text field.
3. Alternatively, enter the IP address and port manually on the classic connection screen.

---

## 8. Physical-Device Validation Procedure

1. **Launch Windows Host**:
   ```powershell
   .\native\windows_engine\build\secondscreen_engine.exe --name "Dev-PC" --show-qr
   ```
2. **Observe QR in Terminal**:
   - Terminal renders ANSI QR code and writes `secondscreen_pair.html`.
   - Press `[q]` to re-display QR code, `[r]` to regenerate token.
3. **Launch Android App**:
   - Open SecondScreen app on physical device.
   - Home screen displays "Saved PCs" with empty state.
4. **Pair Device**:
   - Tap "Add PC" or the QR icon.
   - Grant Camera permission when requested.
   - Point camera at the terminal QR code or `secondscreen_pair.html`.
   - Modal shows verified PC name and candidate IP.
   - Tap "Save PC".
5. **Verify Saved PC Card**:
   - Card displays with a pulsing green indicator.
   - Wait 15 seconds to verify heartbeat continuity.
6. **Start Video Streaming**:
   - Tap "Connect" on the PC card.
   - H.264 stream launches instantly using the verified endpoint without re-discovery delays.

