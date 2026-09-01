#pragma once

#include <array>

namespace rf::ui {

struct Rect {
    float x{};
    float y{};
    float w{};
    float h{};

    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return px >= x && py >= y && px <= x + w && py <= y + h;
    }
};

struct HubLayout {
    static constexpr float width = 1600.0f;
    static constexpr float height = 900.0f;

    Rect leftRail{0, 0, 170, 900};
    Rect featured{195, 230, 920, 375};
    Rect continueButton{220, 505, 205, 68};
    Rect newGameButton{445, 505, 230, 68};
    Rect statusPanel{1135, 230, 440, 375};
    Rect newsBar{195, 868, 1380, 26};
    std::array<Rect, 4> nav{};
    std::array<Rect, 5> featureCards{};

    HubLayout();
};

} // namespace rf::ui
