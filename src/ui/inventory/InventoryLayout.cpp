#include "ui/inventory/InventoryLayout.h"

#include <cstddef>

namespace rf::ui::inventory {

InventoryLayout::InventoryLayout() {
    equipmentSlots = {
        Rect{225, 220, 78, 78},
        Rect{225, 320, 78, 78},
        Rect{225, 420, 78, 78},
        Rect{545, 220, 78, 78},
        Rect{545, 320, 78, 78},
        Rect{545, 420, 78, 78},
    };

    constexpr float slot = 78.0f;
    constexpr float gap = 16.0f;
    for (std::size_t row = 0; row < 5; ++row) {
        for (std::size_t col = 0; col < 6; ++col) {
            inventorySlots[row * 6 + col] = Rect{
                748.0f + static_cast<float>(col) * (slot + gap),
                405.0f + static_cast<float>(row) * (slot + 8.0f),
                slot,
                slot,
            };
        }
    }

    for (std::size_t i = 0; i < hotbarSlots.size(); ++i) {
        hotbarSlots[i] = Rect{389.0f + static_cast<float>(i) * 91.0f, 789.0f, 74.0f, 58.0f};
    }

    craftSlots = {
        Rect{1052, 254, 60, 60},
        Rect{1122, 254, 60, 60},
        Rect{1192, 254, 60, 60},
        Rect{1272, 254, 60, 60},
    };
}

} // namespace rf::ui::inventory
