#pragma once

#include "ui/HubLayout.h"

#include <array>

namespace rf::ui::inventory {

struct InventoryLayout {
    static constexpr float width = 1600.0f;
    static constexpr float height = 900.0f;

    Rect backdrop{0, 0, width, height};
    Rect outerFrame{145, 58, 1310, 785};
    Rect titlePlate{515, 66, 570, 62};
    Rect equipmentPanel{185, 145, 475, 620};
    Rect inventoryPanel{700, 145, 715, 620};
    Rect characterViewport{320, 215, 210, 390};
    Rect hotbarPanel{360, 780, 880, 74};
    Rect quickCraftPanel{1010, 205, 350, 170};

    std::array<Rect, 6> equipmentSlots{};
    std::array<Rect, 30> inventorySlots{};
    std::array<Rect, 9> hotbarSlots{};
    std::array<Rect, 4> craftSlots{};

    InventoryLayout();
};

} // namespace rf::ui::inventory
