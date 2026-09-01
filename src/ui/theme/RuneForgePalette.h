#pragma once
#ifdef _WIN32
#include <d2d1.h>
namespace rf::ui::theme {
inline D2D1_COLOR_F color(unsigned hex, float alpha = 1.0f) noexcept {
    return D2D1::ColorF(static_cast<float>((hex >> 16) & 0xff) / 255.0f,
                        static_cast<float>((hex >> 8) & 0xff) / 255.0f,
                        static_cast<float>(hex & 0xff) / 255.0f,
                        alpha);
}
inline constexpr unsigned Void = 0x04070d;
inline constexpr unsigned Panel = 0x0a1019;
inline constexpr unsigned PanelRaised = 0x111b29;
inline constexpr unsigned BronzeDark = 0x60431f;
inline constexpr unsigned Bronze = 0xb78436;
inline constexpr unsigned Gold = 0xe3b85c;
inline constexpr unsigned GoldBright = 0xffdf82;
inline constexpr unsigned Ivory = 0xf1ead8;
inline constexpr unsigned Muted = 0x9ca6b2;
inline constexpr unsigned BlueGem = 0x2d8eff;
inline constexpr unsigned BlueGlow = 0x75c3ff;
inline constexpr unsigned Emerald = 0x4fd175;
}
#endif
