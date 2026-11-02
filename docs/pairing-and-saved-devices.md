# SecondScreen QR-Code Pairing Flow & Saved-Device Persistence Report

## Executive Summary

This report documents the diagnosis, root causes, architectural fixes, and verification for two critical defects in the SecondScreen Android application:
1. **Defect 1**: Scanning the Windows host QR code completed a token handshake but failed to establish the active video stream connection to the Windows host.
2. **Defect 2**: Previously connected PCs disappeared from the saved device list across screens and app restarts, or displayed false historical connectivity status.

Both features have been redesigned, hardened, and verified with 76 passing automated tests and end-to-end integration flows.

---

## 1. Root Cause Analysis

### Defect 1: QR Scanning Did Not Connect Android App to Stream
1. **Disconnected Navigation Flow**:
   - `HomeScreen._openQrScanner` invoked `Navigator.push(context, MaterialPageRoute(builder: (context) => QrScannerScreen(storage: _storage)))` without awaiting the returned `SavedPc` object.
   - When the user tapped "Done" in `QrScannerScreen`, the screen popped back to `HomeScreen`, but never initiated a TCP stream connection on port `52025` or navigated to `StreamingScreen`.
2. **Single-Port Pairing Restriction**:
   - `QrPairingService._attemptPairOnEndpoint` connected solely to `payload.pairPort` (52027). When the host was running via `SecondScreenEngine.exe` (command line / standalone mode) rather than the GUI host manager, port 52027 was closed and only the main engine port (52025) was active, causing the pairing handshake to fail.
3. **No Camera Retry on Permission Denial**:
   - `QrScannerScreen` lacked a camera retry mechanism. If the user initially denied camera permissions and later granted them in Android app settings, the scanner remained blocked in an error state until the entire app was killed and restarted.
4. **Scanner Dead-End on Error**:
   - Dismissing an error dialog previously left `_isProcessing = true`, permanently preventing subsequent QR code detection without leaving the screen.

### Defect 2: Saved PCs Disappearing & Flawed Persistence
1. **Detached Orphan Storage in `ConnectionScreen`**:
   - `ConnectionScreen`'s AppBar QR icon launched `QrScannerScreen(storage: SavedPcStorage()..load())`. This created an isolated in-memory storage instance that had no listeners attached to `HomeScreen` or `app.dart`. Devices saved from `ConnectionScreen` never updated the visible list on `HomeScreen`.
2. **Unsaved Manual & Discovered Connections**:
   - In `ConnectionScreen._handleConnect`, successful manual IP connections and discovery-based connections pushed `StreamingScreen` but never called `savePc()` or notified `SavedPcStorage`. When the user left the stream, the connected PC was completely absent from the saved list.
3. **Missing Persistence in `updatePcStatus`**:
   - In `SavedPcStorage.updatePcStatus`, when a PC's IP address changed (e.g. via DHCP reassignment or roaming between Wi-Fi bands), the endpoint array was updated in memory, but `_persist()` was never invoked. Force-closing and reopening the app reverted the PC to its stale, unreachable address.
4. **False Historical Connectivity on Launch**:
   - `SavedPcStorage.load()` deserialized historical `status` strings from SharedPreferences (e.g. `online` or `streaming`). PCs that were currently turned off or disconnected were falsely rendered with a green "Online" indicator until a health check executed.

---

## 2. Implemented Architecture & Fixes

### A. Dual-Port Network Pairing Engine (`qr_pairing_service.dart`)
- **Port Fallback**: `_attemptPairOnEndpoint` probes candidate ports in sequence:
  1. `payload.pairPort` (default: 52027 — GUI host manager).
  2. `payload.port` (default: 52025 — standalone streaming engine).
- **Explicit Rejection Halt**: If the host explicitly responds with status `1` (Invalid Token) or `2` (Expired Token), the handshake terminates immediately with an actionable message rather than burning time retrying alternate ports.
- **Dependency Injection**: Added `SocketConnector? socketConnector` for automated mock-socket testing.

### B. Seamless Scan-to-Stream Handoff (`qr_scanner_screen.dart`)
- Accepts `ConnectionService? connectionService` and `SettingsService? settingsService`.
- Upon successful pairing handshake:
  1. Saves the PC into `SavedPcStorage`.
  2. Automatically calls `connectionService.connectToDevice()` to the verified reachable endpoint on port 52025.
  3. Displays a confirmation dialog with connection status and an **"Open Display"** action.
  4. Returns `savedPc` to the caller.
- If dismissed, the caller (`HomeScreen` or `ConnectionScreen`) immediately opens the live display viewport.
- Added **"Retry Camera"** button in `_buildCameraFallback` so users can re-engage the camera after granting runtime permissions without exiting the screen.
- Resetting `_isProcessing = false` on dialog dismissal guarantees instant re-scanning.

### C. Persistent Device Storage (`saved_pc_storage.dart`)
- **Derived Status on App Launch**:
  - `load()` maps all restored PCs to `status: PcOnlineStatus.unknown`.
  - `PcHealthService` immediately runs an active `PACKET_TYPE_HEARTBEAT` probe on startup to accurately derive whether the PC is `online` or `offline`.
- **Durable DHCP Endpoint Tracking**:
  - When a PC responds on a secondary endpoint or updates its IP address, `updatePcStatus` re-prioritizes the active address to `endpoints.first` and persists the updated array to disk via `_persist()`.
- **Storage Injection in Connection Screens**:
  - `ConnectionScreen` receives the shared `SavedPcStorage` instance from `HomeScreen`.
  - Both manual IP connections and discovery connections save or update the PC in `SavedPcStorage` upon successful connection.

---

## 3. Storage Mechanism

Saved devices are stored locally using Android `SharedPreferences` under the key `secondscreen_saved_pcs_v1`.

### Data Schema
Each record is serialized as a JSON string:
```json
{
  "id": "pc-e4b57497-21a4-44b4-82a9-c12e52467d1d",
  "name": "Aryan-PC",
  "endpoints": ["192.168.1.15", "10.0.0.4"],
  "port": 52025,
  "protocolVersion": 2,
  "lastSeen": "2026-10-31T11:45:00.000Z",
  "lastPingMs": 8,
  "osName": "Windows PC"
}
```

- **Security & Privacy**: Ephemeral pairing tokens (`token`, TTL 300s) are **never** persisted to disk. Storing only non-secret network addresses and PC identifiers prevents replay attacks if an Android device backup is inspected.
- **Deduplication**: Devices are indexed and keyed by their immutable `id` (e.g. `pc-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`). Re-scanning the QR code or connecting from a new IP updates the endpoints array rather than creating duplicates.

---

## 4. Test Results & Verification

### Automated Test Suite Summary

```
Total Suites: 10
Total Flutter Tests: 76 (0 failed)
Total Native CTests: 4 (0 failed)
Overall Result: PASS (100%)
```

### Key Targeted Tests

| Test Suite | Spec File | Key Test Cases | Result |
| :--- | :--- | :--- | :--- |
| **QR Schema Validation** | `test/qr_pairing_test.dart` | Validates canonical JSON, rejects expired tokens, rejects future schema versions, rejects malformed payloads. | **PASS** |
| **QR Network Handshake** | `test/qr_pairing_test.dart` | Successful handshake creates `SavedPc`, candidate port fallback (52027 $\rightarrow$ 52025), rejects expired token (code 2), rejects invalid token (code 1). | **PASS** |
| **Device Persistence** | `test/saved_pc_storage_test.dart` | Saves & reloads across restart, derives status dynamically (unknown on load), persists DHCP IP changes, deduplicates by ID. | **PASS** |
| **Lifecycle & Reconnect** | `test/streaming_lifecycle_test.dart` | Screen wake recovery, background socket drops, keepScreenOn toggling. | **PASS** |
| **Connection & Fallback** | `test/connection_service_test.dart` | SS02 H.264 packet routing, SS01 JPEG fallback, exponential backoff retries. | **PASS** |
| **Windows Host CTest** | `native/windows_engine/build/` | `ProtocolTests`, `CaptureTests`, `EncoderTests`, `DriverTests`. | **PASS** |

---

## 5. End-to-End Physical & Simulation Flow

1. **Launch Windows Host**:
   - Host generates stable UUID `pc-xxxx` and ephemeral token valid for 5 minutes.
   - Host displays QR code in the Windows Manager UI, on the terminal, and exports `secondscreen_pair.html`.
2. **Scan on Android**:
   - Android opens `QrScannerScreen` and reads the QR code.
   - Validates JSON payload schema and ensures token is unexpired.
3. **Pairing Handshake**:
   - Android probes host IP on pairing port `52027` (or fallback `52025`).
   - Host verifies token and replies with `PACKET_TYPE_PAIR_RESP` (status 0).
4. **Persistence & Connection**:
   - Android persists `SavedPc` to `SharedPreferences`.
   - Android immediately connects to streaming port `52025` and opens `StreamingScreen`.
5. **App Force-Close & Reopen**:
   - Android app is swiped away and reopened.
   - `HomeScreen` loads the PC with initial status `unknown`.
   - `PcHealthService` immediately pings port `52025`, updating status indicator to green "Online".
   - Tapping the saved PC card connects and streams without requiring the QR code to be scanned again.

