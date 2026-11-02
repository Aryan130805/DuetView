#pragma once

#include "protocol/protocol_constants.hpp"

#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace secondscreen::input {

class InputInjector {
public:
    InputInjector();
    ~InputInjector();

    InputInjector(const InputInjector&) = delete;
    InputInjector& operator=(const InputInjector&) = delete;

    /// Updates the target display output resolution and bounds.
    void UpdateDisplayMetrics(uint32_t width, uint32_t height, int32_t left = 0, int32_t top = 0);

    /// Injects a pointer event from normalized display coordinates [0.0, 1.0].
    /// Returns true if event was successfully validated and injected.
    bool InjectPointerEvent(uint16_t eventType, uint8_t button, uint8_t modifiers, float normX, float normY);

    /// Injects a vertical/horizontal scroll event from normalized coordinates.
    bool InjectScrollEvent(float normX, float normY, int32_t deltaX, int32_t deltaY);

    /// Injects a keyboard event (key down, key up, or direct Unicode character).
    bool InjectKeyboardEvent(uint16_t eventType, uint8_t modifiers, uint32_t keyCode, uint32_t unicodeCodepoint);

    /// Releases any currently pressed mouse buttons or keyboard keys.
    /// MUST be called on client disconnect to prevent stuck keys or buttons.
    void ReleaseAll();

    /// Returns currently tracked held mouse buttons mask (Left = 1, Right = 2, Middle = 4).
    [[nodiscard]] uint8_t GetHeldMouseButtons() const noexcept;

    /// Returns count of currently held keys.
    [[nodiscard]] size_t GetHeldKeyCount() const noexcept;

private:
    void EnsureModifiers(uint8_t modifiers);
    void ReleaseModifiers();
    bool MapNormalizedToScreen(float normX, float normY, int32_t& outScreenX, int32_t& outScreenY, int32_t& outAbsX, int32_t& outAbsY);

    mutable std::mutex mutex_;
    uint32_t displayWidth_{1920};
    uint32_t displayHeight_{1080};
    int32_t displayLeft_{0};
    int32_t displayTop_{0};

    uint8_t heldMouseButtons_{0};
    std::unordered_set<uint32_t> heldKeys_;
    uint8_t activeModifiers_{0};
};

} // namespace secondscreen::input

