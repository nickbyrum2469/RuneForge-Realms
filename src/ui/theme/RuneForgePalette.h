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
inline constexpr unsigned Void = 0x03050a;
inline constexpr unsigned Backdrop = 0x090b0f;
inline constexpr unsigned Panel = 0x0d0f12;
inline constexpr unsigned PanelRaised = 0x17191d;
inline constexpr unsigned PanelInset = 0x090c11;
inline constexpr unsigned BronzeDeep = 0x3b260f;
inline constexpr unsigned BronzeDark = 0x68431b;
inline constexpr unsigned Bronze = 0xa96d25;
inline constexpr unsigned GoldDark = 0x9b671f;
inline constexpr unsigned Gold = 0xd79b38;
inline constexpr unsigned GoldBright = 0xffcf68;
inline constexpr unsigned Ivory = 0xf3ead4;
inline constexpr unsigned Silver = 0xc7c5bf;
inline constexpr unsigned Muted = 0x9ca6b2;
inline constexpr unsigned BlueDeep = 0x074b91;
inline constexpr unsigned BlueGem = 0x087ee8;
inline constexpr unsigned BlueGlow = 0x63c7ff;
inline constexpr unsigned BlueCore = 0xb7eeff;
inline constexpr unsigned Emerald = 0x4fd175;
inline constexpr unsigned Danger = 0xd65a48;
}
#endif
