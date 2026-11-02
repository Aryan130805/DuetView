#include "capture/cursor_manager.hpp"

#include <algorithm>
#include <cstring>

namespace secondscreen::capture {

CursorManager::CursorManager() {
    GenerateDefaultArrowCursor();
}

CursorManager::~CursorManager() = default;

void CursorManager::GenerateDefaultArrowCursor() {
    cursorWidth_ = 32;
    cursorHeight_ = 32;
    hotSpotX_ = 0;
    hotSpotY_ = 0;
    cursorBgra_.assign(cursorWidth_ * cursorHeight_ * 4, 0);

    // Standard high-visibility arrow pointer bitmap mask
    // 0 = Transparent, 1 = Black border, 2 = White interior, 3 = Drop shadow
    static constexpr const char* ARROW_MAP[32] = {
        "1                               ",
        "11                              ",
        "121                             ",
        "1221                            ",
        "12221                           ",
        "122221                          ",
        "1222221                         ",
        "12222221                        ",
        "122222221                       ",
        "1222222221                      ",
        "12222222221                     ",
        "122222222221                    ",
        "1222222111111                   ",
        "122212213333                    ",
        "1221 1221                       ",
        "121  1221                       ",
        "11    1221                      ",
        "1     1221                      ",
        "       1221                     ",
        "       1221                     ",
        "        11                      ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                "
    };

    for (uint32_t y = 0; y < 32; ++y) {
        for (uint32_t x = 0; x < 32; ++x) {
            const char c = ARROW_MAP[y][x];
            const size_t idx = (y * 32 + x) * 4;

            if (c == '1') {
                // Solid black outline (B, G, R, A)
                cursorBgra_[idx + 0] = 0;
                cursorBgra_[idx + 1] = 0;
                cursorBgra_[idx + 2] = 0;
                cursorBgra_[idx + 3] = 255;
            } else if (c == '2') {
                // Crisp white interior
                cursorBgra_[idx + 0] = 255;
                cursorBgra_[idx + 1] = 255;
                cursorBgra_[idx + 2] = 255;
                cursorBgra_[idx + 3] = 255;
            } else if (c == '3') {
                // Subtle drop shadow
                cursorBgra_[idx + 0] = 0;
                cursorBgra_[idx + 1] = 0;
                cursorBgra_[idx + 2] = 0;
                cursorBgra_[idx + 3] = 100;
            } else {
                cursorBgra_[idx + 0] = 0;
                cursorBgra_[idx + 1] = 0;
                cursorBgra_[idx + 2] = 0;
                cursorBgra_[idx + 3] = 0;
            }
        }
    }
}

void CursorManager::UpdateCursorShape(HCURSOR hCursor) {
    if (hCursor == lastCursor_ && !cursorBgra_.empty()) {
        return;
    }
    lastCursor_ = hCursor;

    if (!hCursor) {
        GenerateDefaultArrowCursor();
        return;
    }

    ICONINFO ii{};
    if (!GetIconInfo(hCursor, &ii)) {
        GenerateDefaultArrowCursor();
        return;
    }

    hotSpotX_ = static_cast<int32_t>(ii.xHotspot);
    hotSpotY_ = static_cast<int32_t>(ii.yHotspot);

    BITMAP bm{};
    if (ii.hbmColor && GetObject(ii.hbmColor, sizeof(BITMAP), &bm)) {
        cursorWidth_ = static_cast<uint32_t>(bm.bmWidth);
        cursorHeight_ = static_cast<uint32_t>(bm.bmHeight);
    } else if (ii.hbmMask && GetObject(ii.hbmMask, sizeof(BITMAP), &bm)) {
        cursorWidth_ = static_cast<uint32_t>(bm.bmWidth);
        cursorHeight_ = static_cast<uint32_t>(bm.bmHeight / (ii.hbmColor ? 1 : 2));
    } else {
        cursorWidth_ = 32;
        cursorHeight_ = 32;
    }

    if (cursorWidth_ == 0 || cursorHeight_ == 0 || cursorWidth_ > 256 || cursorHeight_ > 256) {
        cursorWidth_ = 32;
        cursorHeight_ = 32;
        GenerateDefaultArrowCursor();
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        return;
    }

    // Render icon to 32-bit DIB section
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(cursorWidth_);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(cursorHeight_); // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hDIB = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);

    if (hDIB && pBits) {
        HGDIOBJ oldBmp = SelectObject(hdcMem, hDIB);
        std::memset(pBits, 0, cursorWidth_ * cursorHeight_ * 4);

        DrawIconEx(hdcMem, 0, 0, hCursor, static_cast<int>(cursorWidth_), static_cast<int>(cursorHeight_), 0, nullptr, DI_NORMAL);

        cursorBgra_.resize(cursorWidth_ * cursorHeight_ * 4);
        std::memcpy(cursorBgra_.data(), pBits, cursorBgra_.size());

        // Check if alpha channel is populated; if not, determine alpha from color/mask
        bool hasNonZeroAlpha = false;
        for (size_t i = 3; i < cursorBgra_.size(); i += 4) {
            if (cursorBgra_[i] > 0) {
                hasNonZeroAlpha = true;
                break;
            }
        }

        if (!hasNonZeroAlpha) {
            // Monochromatic cursor or GDI bitmap without alpha; synthesize alpha from pixels
            for (size_t i = 0; i < cursorBgra_.size(); i += 4) {
                const uint8_t b = cursorBgra_[i + 0];
                const uint8_t g = cursorBgra_[i + 1];
                const uint8_t r = cursorBgra_[i + 2];
                if (b != 0 || g != 0 || r != 0) {
                    cursorBgra_[i + 3] = 255;
                }
            }
        }

        SelectObject(hdcMem, oldBmp);
        DeleteObject(hDIB);
    } else {
        GenerateDefaultArrowCursor();
    }

    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
}

bool CursorManager::CompositeCursor(uint8_t* bgraFrame,
                                    uint32_t frameWidth,
                                    uint32_t frameHeight,
                                    uint32_t frameStride,
                                    int32_t displayLeft,
                                    int32_t displayTop) {
    if (!bgraFrame || frameWidth == 0 || frameHeight == 0) {
        return false;
    }

    CURSORINFO ci{};
    ci.cbSize = sizeof(CURSORINFO);
    if (!GetCursorInfo(&ci)) {
        return false;
    }

    if (!(ci.flags & CURSOR_SHOWING)) {
        return false;
    }

    UpdateCursorShape(ci.hCursor);

    // Calculate top-left location of cursor bitmap relative to target display
    const int32_t cursorScreenX = ci.ptScreenPos.x - displayLeft - hotSpotX_;
    const int32_t cursorScreenY = ci.ptScreenPos.y - displayTop - hotSpotY_;

    const int32_t dstStartX = std::max(0, cursorScreenX);
    const int32_t dstStartY = std::max(0, cursorScreenY);
    const int32_t dstEndX = std::min(static_cast<int32_t>(frameWidth), cursorScreenX + static_cast<int32_t>(cursorWidth_));
    const int32_t dstEndY = std::min(static_cast<int32_t>(frameHeight), cursorScreenY + static_cast<int32_t>(cursorHeight_));

    if (dstStartX >= dstEndX || dstStartY >= dstEndY) {
        return false; // Cursor is outside the captured display
    }

    const int32_t srcOffsetX = dstStartX - cursorScreenX;
    const int32_t srcOffsetY = dstStartY - cursorScreenY;
    const uint32_t copyWidth = static_cast<uint32_t>(dstEndX - dstStartX);
    const uint32_t copyHeight = static_cast<uint32_t>(dstEndY - dstStartY);

    for (uint32_t row = 0; row < copyHeight; ++row) {
        uint8_t* pDst = bgraFrame + ((dstStartY + row) * frameStride) + (dstStartX * 4);
        const uint8_t* pSrc = cursorBgra_.data() + (((srcOffsetY + row) * cursorWidth_ + srcOffsetX) * 4);

        for (uint32_t col = 0; col < copyWidth; ++col) {
            const uint32_t alpha = pSrc[3];
            if (alpha == 255) {
                pDst[0] = pSrc[0];
                pDst[1] = pSrc[1];
                pDst[2] = pSrc[2];
            } else if (alpha > 0) {
                const uint32_t invAlpha = 255 - alpha;
                pDst[0] = static_cast<uint8_t>((pSrc[0] * alpha + pDst[0] * invAlpha) / 255);
                pDst[1] = static_cast<uint8_t>((pSrc[1] * alpha + pDst[1] * invAlpha) / 255);
                pDst[2] = static_cast<uint8_t>((pSrc[2] * alpha + pDst[2] * invAlpha) / 255);
            }
            pDst += 4;
            pSrc += 4;
        }
    }

    return true;
}

} // namespace secondscreen::capture

