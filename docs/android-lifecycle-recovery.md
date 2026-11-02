# SecondScreen Android Application: Lifecycle Recovery and Stream Reconnection

## Executive Summary

When the Android screen goes black, turns off, or the device is locked, Android tears down the underlying rendering `Surface` and restricts network sockets under system power-management and Doze policies. In previous versions of SecondScreen, this led to an unrecoverable "Disconnected from host" state that required manual intervention or app restarts.

This update resolves the issue through:
1. **Preventive Screen-On Management (`FLAG_KEEP_SCREEN_ON`)** during active streaming sessions.
2. **Dynamic Surface Lifecycle Reattachment** with immediate IDR keyframe re-requesting in the native MediaCodec decoder.
3. **Application Lifecycle State Tracking (`WidgetsBindingObserver`)** in the Flutter viewport.
4. **Reliable Automatic Reconnection with Bounded Exponential Backoff and Jitter** without wiping the target PC identity.

---

## 1. Root Cause Analysis

Before this implementation, four interrelated factors caused connection failure upon screen turn-off:

| Component | Pre-Fix Behavior | Root Cause Impact |
| :--- | :--- | :--- |
| **Android Window Management** | No `FLAG_KEEP_SCREEN_ON` flag applied to the window during display streaming. | If the user was watching the screen without touching (e.g. reading a document on the secondary monitor), Android's screen timeout turned off the display. |
| **`SurfaceView` & `H264Decoder`** | When the screen turned off, `VideoSurfacePlatformView.surfaceDestroyed()` detached the surface (`decoder.setSurface(null)`). When the screen woke, `surfaceCreated()` reattached the surface, but `H264Decoder` did not request an IDR keyframe. | The decoder had a valid surface but remained unable to render incoming delta frames without an IDR keyframe. |
| **`StreamingScreen` UI Lifecycle** | Did not implement `WidgetsBindingObserver`. | Ignored Android lifecycle transitions (`AppLifecycleState.paused` / `resumed`). When the user unlocked their phone, the screen remained stuck in the error state. |
| **`ConnectionService` State Persistence** | On socket error or EOF (`onDone` / `onError`), `disconnect(reason: ...)` set `connectedDevice = null`. | Dropping the connection wiped the remote PC metadata (`address` and `port`), preventing automatic background reconnection or one-tap user retries. |

---

## 2. Technical Architecture & State Machine

```
                   [Streaming State]
                          │
            Screen Off / Phone Locks (Paused)
                          │
          ┌───────────────┴───────────────┐
          ▼                               ▼
   [Socket Remains Alive]          [Socket Dropped / Suspended]
   - Surface detached              - Surface detached
   - Buffer draining paused        - ConnectionService enters failed state
          │                        - Target PC preserved in lastConnectedDevice
          │                               │
   Screen On / Unlocked (Resumed)  Screen On / Unlocked (Resumed)
          │                               │
   - Surface reattached            - ConnectionService.reconnect()
   - Decoder requests keyframe       with exponential backoff + jitter
   - Upstream SS02 Keyframe Req    - TCP Socket re-established
          │                        - Upstream SS02 Keyframe Req
          └───────────────┬───────────────┘
                          ▼
            [Active Streaming Restored]
```

---

## 3. Implementation Details

### A. Window Screen-Awake Flag (`FLAG_KEEP_SCREEN_ON`)
- Added `setKeepScreenOn` method channel call in `SecondScreenNativeBridge.kt`:
  - When streaming screen opens: adds `WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON` to the activity window.
  - When user exits streaming screen or app is disposed: clears `FLAG_KEEP_SCREEN_ON`.
  - Does not require dangerous `WAKE_LOCK` manifest permissions, adhering to modern Android power guidelines.

### B. Native MediaCodec Decoder Recovery (`H264Decoder.kt`)
- In `H264Decoder.setSurface(surface: Surface?)`:
  - When a valid non-null surface is reattached upon screen wake, the decoder automatically invokes `listener?.onKeyframeRequested()`.
  - Dispatches an upstream `PACKET_TYPE_KEYFRAME_REQ` (0x0006) to the Windows host so the restored SurfaceView is populated immediately with fresh video data.

### C. Persistent Target Identity & Reconnection Engine (`ConnectionService.dart`)
- **Metadata Retention:**
  - Added `lastConnectedDevice` getter to `ConnectionService`.
  - When an unexpected disconnect occurs (`reason != null`), the target PC identity is preserved in `state.connectedDevice` and `lastConnectedDevice`.
- **Bounded Exponential Backoff with Jitter:**
  - Implemented `reconnect({int maxAttempts = 5, Duration initialDelay = 500ms})`.
  - Backoff delay formula: `clamp(initialDelay * 2^(attempt - 2), initialDelay, 5000ms) + jitterMs`.
  - Duplicate-call guard (`_isReconnecting` flag) guarantees that rapid wake/sleep cycles do not spawn conflicting connection loops.

### D. Viewport Lifecycle Handling (`StreamingScreen.dart`)
- Extended `_StreamingScreenState` with `WidgetsBindingObserver`:
  - `didChangeAppLifecycleState(AppLifecycleState state)`:
    - On `resumed`: checks `connectionService.isSocketConnected`.
      - If socket is alive: triggers `sendKeyframeRequest()`.
      - If socket was severed during sleep: invokes `_triggerAutoReconnect()`.
    - Automatically restores `SystemUiMode.immersiveSticky` and `FLAG_KEEP_SCREEN_ON`.
- Updated viewport UI to show an animated reconnecting spinner (`Reconnecting to PC... Restoring display stream after pause/wake`) instead of a dead error screen.

---

## 4. Verification and Test Results

### Automated Test Suites

| Test Suite | File | Tests Run | Result |
| :--- | :--- | :--- | :--- |
| **Android Streaming Lifecycle** | `test/streaming_lifecycle_test.dart` | 3 | **PASS** |
| **Connection & Reconnection** | `test/connection_service_test.dart` | 13 | **PASS** |
| **Decoder Bridge & Screen-On** | `test/decoder_bridge_test.dart` | 8 | **PASS** |
| **Full Flutter Test Suite** | `apps/android_app/test/` | 69 | **PASS** |
| **Windows Engine Native CTest** | `native/windows_engine/build/` | 4 (Driver, Capture, Encoder, Protocol) | **PASS** |

### Verified Behaviors

1. **Screen Wake with Healthy Socket:**
   - Simulated pause/resume lifecycle while socket is alive:
   - Successfully dispatches upstream `PACKET_TYPE_KEYFRAME_REQ`.
   - Verified that reconnect call count is 0.
2. **Screen Wake with Severed Socket:**
   - Simulated socket disconnect during device suspension.
   - On resume, automatically triggers `reconnect()`.
   - Backoff retries succeed and restore active streaming.
3. **Overlapping Reconnect Prevention:**
   - Verified that concurrent calls to `reconnect()` while a loop is active return `false` immediately, avoiding socket collisions.
4. **Window Flag Toggling:**
   - Verified that entering `StreamingScreen` invokes `setKeepScreenOn(enable: true)` and leaving/disposing invokes `setKeepScreenOn(enable: false)`.

