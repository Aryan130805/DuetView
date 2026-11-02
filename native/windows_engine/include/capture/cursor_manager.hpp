#pragma once

#include <cstdint>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace secondscreen::capture {

class CursorManager {
public:
    CursorManager();
    ~CursorManager();

    /// Renders the active system cursor directly into a 32-bit BGRA video frame buffer
    /// using accurate alpha blending and bounds clipping.
    /// Returns true if cursor was showing and successfully composited onto the frame.
    bool CompositeCursor(uint8_t* bgraFrame,
                         uint32_t frameWidth,
                         uint32_t frameHeight,
                         uint32_t frameStride,
                         int32_t displayLeft = 0,
                         int32_t displayTop = 0);

    /// Updates the cached cursor bitmap from the current Win32 cursor if it changed.
    void UpdateCursorShape(HCURSOR hCursor);

    [[nodiscard]] uint32_t GetCursorWidth() const noexcept { return cursorWidth_; }
    [[nodiscard]] uint32_t GetCursorHeight() const noexcept { return cursorHeight_; }

private:
    void GenerateDefaultArrowCursor();

    uint32_t cursorWidth_{32};
    uint32_t cursorHeight_{32};
    int32_t hotSpotX_{0};
    int32_t hotSpotY_{0};
    std::vector<uint8_t> cursorBgra_;
    HCURSOR lastCursor_{nullptr};
};

} // namespace secondscreen::capture

