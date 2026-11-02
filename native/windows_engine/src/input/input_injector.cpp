#include "input/input_injector.hpp"

#include <algorithm>
#include <iostream>

namespace secondscreen::input {

InputInjector::InputInjector() = default;

InputInjector::~InputInjector() {
    ReleaseAll();
}

void InputInjector::UpdateDisplayMetrics(uint32_t width, uint32_t height, int32_t left, int32_t top) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (width > 0 && height > 0) {
        displayWidth_ = width;
        displayHeight_ = height;
        displayLeft_ = left;
        displayTop_ = top;
    }
}

bool InputInjector::MapNormalizedToScreen(float normX, float normY,
                                          int32_t& outScreenX, int32_t& outScreenY,
                                          int32_t& outAbsX, int32_t& outAbsY) {
    if (std::isnan(normX) || std::isnan(normY) ||
        std::isinf(normX) || std::isinf(normY) ||
        normX < 0.0f || normX > 1.0f ||
        normY < 0.0f || normY > 1.0f) {
        return false;
    }

    // Map to pixel within this display
    outScreenX = displayLeft_ + static_cast<int32_t>(std::round(normX * static_cast<float>(displayWidth_ - 1)));
    outScreenY = displayTop_ + static_cast<int32_t>(std::round(normY * static_cast<float>(displayHeight_ - 1)));

    // Map to virtual desktop coordinates (0 .. 65535)
    const int32_t virtLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int32_t virtTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int32_t virtWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int32_t virtHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (virtWidth <= 0 || virtHeight <= 0) {
        return false;
    }

    outAbsX = static_cast<int32_t>(std::round(((static_cast<float>(outScreenX - virtLeft) * 65535.0f) / static_cast<float>(virtWidth))));
    outAbsY = static_cast<int32_t>(std::round(((static_cast<float>(outScreenY - virtTop) * 65535.0f) / static_cast<float>(virtHeight))));

    outAbsX = std::clamp(outAbsX, 0, 65535);
    outAbsY = std::clamp(outAbsY, 0, 65535);

    return true;
}

void InputInjector::EnsureModifiers(uint8_t modifiers) {
    const uint8_t needed = modifiers;
    const uint8_t current = activeModifiers_;

    const auto toggleKey = [](WORD vk, bool press) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        in.ki.dwFlags = press ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
    };

    if ((needed & protocol::ModifierFlags::Shift) && !(current & protocol::ModifierFlags::Shift)) {
        toggleKey(VK_SHIFT, true);
    } else if (!(needed & protocol::ModifierFlags::Shift) && (current & protocol::ModifierFlags::Shift)) {
        toggleKey(VK_SHIFT, false);
    }

    if ((needed & protocol::ModifierFlags::Ctrl) && !(current & protocol::ModifierFlags::Ctrl)) {
        toggleKey(VK_CONTROL, true);
    } else if (!(needed & protocol::ModifierFlags::Ctrl) && (current & protocol::ModifierFlags::Ctrl)) {
        toggleKey(VK_CONTROL, false);
    }

    if ((needed & protocol::ModifierFlags::Alt) && !(current & protocol::ModifierFlags::Alt)) {
        toggleKey(VK_MENU, true);
    } else if (!(needed & protocol::ModifierFlags::Alt) && (current & protocol::ModifierFlags::Alt)) {
        toggleKey(VK_MENU, false);
    }

    if ((needed & protocol::ModifierFlags::Meta) && !(current & protocol::ModifierFlags::Meta)) {
        toggleKey(VK_LWIN, true);
    } else if (!(needed & protocol::ModifierFlags::Meta) && (current & protocol::ModifierFlags::Meta)) {
        toggleKey(VK_LWIN, false);
    }

    activeModifiers_ = needed;
}

void InputInjector::ReleaseModifiers() {
    EnsureModifiers(protocol::ModifierFlags::None);
}

bool InputInjector::InjectPointerEvent(uint16_t eventType, uint8_t button, uint8_t modifiers, float normX, float normY) {
    std::lock_guard<std::mutex> lock(mutex_);

    int32_t screenX = 0, screenY = 0, absX = 0, absY = 0;
    if (!MapNormalizedToScreen(normX, normY, screenX, screenY, absX, absY)) {
        return false;
    }

    EnsureModifiers(modifiers);

    const auto pType = static_cast<protocol::PointerEventType>(eventType);
    const auto pButton = static_cast<protocol::PointerButton>(button);

    switch (pType) {
        case protocol::PointerEventType::Move: {
            SetCursorPos(screenX, screenY);
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dx = absX;
            input.mi.dy = absY;
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
            SendInput(1, &input, sizeof(INPUT));
            return true;
        }

        case protocol::PointerEventType::ButtonDown: {
            SetCursorPos(screenX, screenY);
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dx = absX;
            input.mi.dy = absY;
            input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

            if (pButton == protocol::PointerButton::Left) {
                input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
                heldMouseButtons_ |= 0x01;
            } else if (pButton == protocol::PointerButton::Right) {
                input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
                heldMouseButtons_ |= 0x02;
            } else if (pButton == protocol::PointerButton::Middle) {
                input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
                heldMouseButtons_ |= 0x04;
            } else {
                return false;
            }

            SendInput(1, &input, sizeof(INPUT));
            return true;
        }

        case protocol::PointerEventType::ButtonUp: {
            SetCursorPos(screenX, screenY);
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dx = absX;
            input.mi.dy = absY;
            input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

            if (pButton == protocol::PointerButton::Left) {
                input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
                heldMouseButtons_ &= ~0x01;
            } else if (pButton == protocol::PointerButton::Right) {
                input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
                heldMouseButtons_ &= ~0x02;
            } else if (pButton == protocol::PointerButton::Middle) {
                input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
                heldMouseButtons_ &= ~0x04;
            } else {
                return false;
            }

            SendInput(1, &input, sizeof(INPUT));
            return true;
        }

        case protocol::PointerEventType::DoubleClick: {
            SetCursorPos(screenX, screenY);
            INPUT inputDown{};
            inputDown.type = INPUT_MOUSE;
            inputDown.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

            INPUT inputUp{};
            inputUp.type = INPUT_MOUSE;
            inputUp.mi.dwFlags = MOUSEEVENTF_LEFTUP;

            SendInput(1, &inputDown, sizeof(INPUT));
            SendInput(1, &inputUp, sizeof(INPUT));
            Sleep(25);
            SendInput(1, &inputDown, sizeof(INPUT));
            SendInput(1, &inputUp, sizeof(INPUT));
            return true;
        }
    }

    return false;
}

bool InputInjector::InjectScrollEvent(float normX, float normY, int32_t deltaX, int32_t deltaY) {
    std::lock_guard<std::mutex> lock(mutex_);

    int32_t screenX = 0, screenY = 0, absX = 0, absY = 0;
    if (!MapNormalizedToScreen(normX, normY, screenX, screenY, absX, absY)) {
        return false;
    }

    SetCursorPos(screenX, screenY);

    // Move cursor to pointer target before scrolling
    INPUT moveIn{};
    moveIn.type = INPUT_MOUSE;
    moveIn.mi.dx = absX;
    moveIn.mi.dy = absY;
    moveIn.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &moveIn, sizeof(INPUT));

    if (deltaY != 0) {
        INPUT scrollIn{};
        scrollIn.type = INPUT_MOUSE;
        scrollIn.mi.dwFlags = MOUSEEVENTF_WHEEL;
        scrollIn.mi.mouseData = static_cast<DWORD>(deltaY);
        SendInput(1, &scrollIn, sizeof(INPUT));
    }

    if (deltaX != 0) {
        INPUT hScrollIn{};
        hScrollIn.type = INPUT_MOUSE;
        hScrollIn.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        hScrollIn.mi.mouseData = static_cast<DWORD>(deltaX);
        SendInput(1, &hScrollIn, sizeof(INPUT));
    }

    return true;
}

bool InputInjector::InjectKeyboardEvent(uint16_t eventType, uint8_t modifiers, uint32_t keyCode, uint32_t unicodeCodepoint) {
    std::lock_guard<std::mutex> lock(mutex_);

    EnsureModifiers(modifiers);

    const auto kType = static_cast<protocol::KeyEventType>(eventType);

    if (kType == protocol::KeyEventType::UnicodeChar) {
        if (unicodeCodepoint == 0 || unicodeCodepoint > 0x10FFFF) {
            return false;
        }

        // Handle surrogate pairs if > 0xFFFF
        if (unicodeCodepoint > 0xFFFF) {
            const uint32_t uPrime = unicodeCodepoint - 0x10000;
            const WORD highSurrogate = static_cast<WORD>(0xD800 + (uPrime >> 10));
            const WORD lowSurrogate = static_cast<WORD>(0xDC00 + (uPrime & 0x3FF));

            INPUT inputs[4]{};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wScan = highSurrogate;
            inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wScan = lowSurrogate;
            inputs[1].ki.dwFlags = KEYEVENTF_UNICODE;

            inputs[2].type = INPUT_KEYBOARD;
            inputs[2].ki.wScan = highSurrogate;
            inputs[2].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

            inputs[3].type = INPUT_KEYBOARD;
            inputs[3].ki.wScan = lowSurrogate;
            inputs[3].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

            SendInput(4, inputs, sizeof(INPUT));
            return true;
        }

        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = static_cast<WORD>(unicodeCodepoint);
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wScan = static_cast<WORD>(unicodeCodepoint);
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

        SendInput(2, inputs, sizeof(INPUT));
        return true;
    }

    if (kType == protocol::KeyEventType::KeyDown) {
        if (keyCode == 0 || keyCode > 255) {
            return false;
        }
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(keyCode);
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(INPUT));
        heldKeys_.insert(keyCode);
        return true;
    }

    if (kType == protocol::KeyEventType::KeyUp) {
        if (keyCode == 0 || keyCode > 255) {
            return false;
        }
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(keyCode);
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        heldKeys_.erase(keyCode);
        return true;
    }

    return false;
}

void InputInjector::ReleaseAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Release mouse buttons
    if (heldMouseButtons_ & 0x01) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &in, sizeof(INPUT));
    }
    if (heldMouseButtons_ & 0x02) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(1, &in, sizeof(INPUT));
    }
    if (heldMouseButtons_ & 0x04) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        SendInput(1, &in, sizeof(INPUT));
    }
    heldMouseButtons_ = 0;

    // Release any held virtual keys
    for (uint32_t vk : heldKeys_) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
    }
    heldKeys_.clear();

    // Release modifiers
    ReleaseModifiers();
}

uint8_t InputInjector::GetHeldMouseButtons() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return heldMouseButtons_;
}

size_t InputInjector::GetHeldKeyCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return heldKeys_.size();
}

} // namespace secondscreen::input

