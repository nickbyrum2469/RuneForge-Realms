#pragma once

#include "game/items/ItemId.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rf::game::inventory {

struct ItemStack {
    items::ItemId item{items::ItemId::None};
    std::uint16_t count{};

    [[nodiscard]] bool empty() const noexcept { return item == items::ItemId::None || count == 0; }
};

class Inventory {
public:
    static constexpr std::size_t hotbarSize = 9;
    static constexpr std::size_t backpackSize = 27;
    static constexpr std::size_t slotCount = hotbarSize + backpackSize;
    static constexpr std::uint16_t defaultMaxStack = 64;

    [[nodiscard]] const std::array<ItemStack, slotCount>& slots() const noexcept { return slots_; }
    [[nodiscard]] const ItemStack& slot(std::size_t index) const noexcept;
    [[nodiscard]] ItemStack& slot(std::size_t index) noexcept;

    [[nodiscard]] std::uint16_t add(items::ItemId item, std::uint16_t count,
                                    std::uint16_t maxStack = defaultMaxStack) noexcept;
    [[nodiscard]] bool removeFromSlot(std::size_t index, std::uint16_t count = 1) noexcept;
    void clear() noexcept;

    void selectHotbar(std::size_t index) noexcept;
    [[nodiscard]] std::size_t selectedHotbar() const noexcept { return selectedHotbar_; }
    [[nodiscard]] const ItemStack& selectedStack() const noexcept { return slots_[selectedHotbar_]; }

private:
    std::array<ItemStack, slotCount> slots_{};
    std::size_t selectedHotbar_{};
};

} // namespace rf::game::inventory
