#include "ui/inventory/InventoryLayout.h"

#include <cstddef>

namespace rf::ui::inventory {

InventoryLayout::InventoryLayout() {
    equipmentSlots = {
        Rect{205, 245, 76, 76},
        Rect{205, 345, 76, 76},
        Rect{205, 445, 76, 76},
        Rect{552, 245, 76, 76},
        Rect{552, 345, 76, 76},
        Rect{552, 445, 76, 76},
    };

    constexpr float slot = 76.0f;
    constexpr float gapX = 17.0f;
    constexpr float gapY = 10.0f;
    for (std::size_t row = 0; row < 5; ++row) {
        for (std::size_t col = 0; col < 6; ++col) {
            inventorySlots[row * 6 + col] = Rect{
                725.0f + static_cast<float>(col) * (slot + gapX),
                390.0f + static_cast<float>(row) * (slot + gapY),
                slot,
                slot,
            };
        }
    }

    for (std::size_t i = 0; i < hotbarSlots.size(); ++i) {
        hotbarSlots[i] = Rect{382.0f + static_cast<float>(i) * 92.0f, 795.0f, 76.0f, 56.0f};
    }

    craftSlots = {
        Rect{1048, 285, 58, 58},
        Rect{1125, 285, 58, 58},
        Rect{1202, 285, 58, 58},
        Rect{1279, 285, 58, 58},
    };
}

} // namespace rf::ui::inventory
