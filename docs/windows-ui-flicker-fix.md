# SecondScreen Windows UI Flicker Diagnostic & Resolution Report

## Executive Summary
This report documents the root cause analysis, architecture updates, and verification results for resolving persistent UI flickering in the SecondScreen Windows desktop management application (`SecondScreenApp.exe`).

---

## 1. Root Cause Analysis

### Defect 1: Unhandled `WM_ERASEBKGND`
- **Mechanism**: In Win32 GDI applications, when a window region is invalidated, Windows issues `WM_ERASEBKGND` before `WM_PAINT`. In the absence of an explicit message handler, `DefWindowProcW` fills the invalid rect with the registered class brush (`wc.hbrBackground`).
- **Effect**: On every timer interval, the window canvas was filled with dark slate background before cards, text labels, and QR modules were rendered on top, creating visible strobe-like flashing.

### Defect 2: Direct live screen DC rendering without double buffering
- **Mechanism**: The original `WM_PAINT` handler drew directly to the display device context (`hdc` from `BeginPaint`).
- **Effect**: Each frame involved multiple `RoundRect` calls, card borders, text lines, and dozens of discrete `FillRect` calls for QR code bit modules. The raster scan exposed intermediate drawing steps directly to the monitor surface.

### Defect 3: Missing `WS_CLIPCHILDREN` window style
- **Mechanism**: The main window was created with `WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`. Without `WS_CLIPCHILDREN`, parent paint cycles drew directly underneath child controls (edit boxes, buttons, comboboxes), causing child controls to be clipped and repainted repeatedly.

### Defect 4: Periodic Full-Window Invalidation in 1-Second Timer
- **Mechanism**: In `RefreshDriverStatus()`, `InvalidateRect(g_hWnd, nullptr, FALSE)` was called every 3 seconds regardless of whether driver state had changed, and `ShowWindow`/`SetWindowTextW` were re-applied unconditionally to buttons.
- **Effect**: Periodic redraws of the whole window even while completely idle.

### Defect 5: Countdown timer invalidation bounding box mismatch
- **Mechanism**: In `WM_TIMER`, the countdown timer was invalidated using `{ 415, 318, 755, 340 }`, whereas the actual text was drawn at `{ 415, 305, 795, 325 }`.
- **Effect**: Partial text redraw artifacts and redraw jitter.

---

## 2. Technical Implementation

1. **`WM_ERASEBKGND` Suppression**:
   ```cpp
   case WM_ERASEBKGND:
       // Background is drawn atomically in WM_PAINT via offscreen backbuffer
       return 1;
   ```

2. **Offscreen Double Buffering**:
   - In `WM_PAINT`, an offscreen memory DC (`CreateCompatibleDC`) and bitmap (`CreateCompatibleBitmap`) are allocated.
   - The entire frame (background, header, badge, configuration card, QR card, QR matrix, countdown text, driver banner) is rendered into `memDC`.
   - A single atomic `BitBlt` transfers the composed frame onto the display screen DC without intermediate tearing or flashing.

3. **Window Clipping**:
   - Added `WS_CLIPCHILDREN` to `CreateWindowExW` in `WinMain`.

4. **Change-Driven Driver Status Refresh**:
   - `RefreshDriverStatus()` caches `static DriverState lastReportedState` and only updates UI controls or invalidates the driver banner rect (`{ 395, 415, 815, 485 }`) when an actual state transition occurs.

5. **Precise Invalidation Bounds**:
   - `rcQrTime` adjusted to `{ 415, 305, 795, 328 }` to match the exact text geometry.

---

## 3. Verification & Acceptance Status

| Criterion | Target | Status |
| :--- | :--- | :---: |
| AC-1: Zero background strobe | Background flicker eliminated via `WM_ERASEBKGND` and memory backbuffer | **PASS** |
| AC-2: Child control stability | Buttons and combo boxes do not flicker or vanish during redraws | **PASS** |
| AC-3: Idle CPU & paint efficiency | Invalidation occurs strictly on changed timer bounds | **PASS** |
| AC-4: Native compilation | `SecondScreenApp.exe` builds cleanly with CMake / MSVC | **PASS** |
| AC-5: CTest regression suite | All 4 native test suites pass | **PASS** |

