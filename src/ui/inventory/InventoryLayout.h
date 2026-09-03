#pragma once

#include "ui/HubLayout.h"

#include <array>

namespace rf::ui::inventory {

struct InventoryLayout {
    static constexpr float width = 1600.0f;
    static constexpr float height = 900.0f;

    Rect backdrop{0, 0, width, height};
    Rect outerFrame{120, 42, 1360, 820};
    Rect titlePlate{505, 56, 590, 86};
    Rect equipmentPanel{165, 165, 480, 595};
    Rect inventoryPanel{680, 165, 755, 595};
    Rect characterViewport{320, 245, 205, 335};
    Rect hotbarPanel{335, 785, 930, 76};
    Rect quickCraftPanel{1025, 235, 360, 142};

    std::array<Rect, 6> equipmentSlots{};
    std::array<Rect, 30> inventorySlots{};
    std::array<Rect, 9> hotbarSlots{};
    std::array<Rect, 4> craftSlots{};

    InventoryLayout();
};

} // namespace rf::ui::inventory
