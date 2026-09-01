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
    Rect featured{195, 245, 1030, 350};
    Rect playButton{215, 510, 230, 70};
    Rect partyPanel{1260, 245, 315, 350};
    Rect newsBar{195, 868, 1380, 26};
    std::array<Rect, 6> nav{};
    std::array<Rect, 6> cards{};

    HubLayout();
};

} // namespace rf::ui
