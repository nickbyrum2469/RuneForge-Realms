#include "game/inventory/Inventory.h"

#include <algorithm>

namespace rf::game::inventory {
namespace {
ItemStack emptyStack{};
}

const ItemStack& Inventory::slot(std::size_t index) const noexcept {
    return index < slots_.size() ? slots_[index] : emptyStack;
}

ItemStack& Inventory::slot(std::size_t index) noexcept {
    return index < slots_.size() ? slots_[index] : emptyStack;
}

std::uint16_t Inventory::add(items::ItemId item, std::uint16_t count, std::uint16_t maxStack) noexcept {
    if (item == items::ItemId::None || count == 0 || maxStack == 0) return count;

    for (auto& stack : slots_) {
        if (stack.item != item || stack.count >= maxStack) continue;
        const auto capacity = static_cast<std::uint16_t>(maxStack - stack.count);
        const auto moved = std::min(count, capacity);
        stack.count = static_cast<std::uint16_t>(stack.count + moved);
        count = static_cast<std::uint16_t>(count - moved);
        if (count == 0) return 0;
    }

    for (auto& stack : slots_) {
        if (!stack.empty()) continue;
        const auto moved = std::min(count, maxStack);
        stack.item = item;
        stack.count = moved;
        count = static_cast<std::uint16_t>(count - moved);
        if (count == 0) return 0;
    }
    return count;
}

bool Inventory::removeFromSlot(std::size_t index, std::uint16_t count) noexcept {
    if (index >= slots_.size() || count == 0) return false;
    auto& stack = slots_[index];
    if (stack.empty() || stack.count < count) return false;
    stack.count = static_cast<std::uint16_t>(stack.count - count);
    if (stack.count == 0) stack = {};
    return true;
}

void Inventory::clear() noexcept {
    slots_ = {};
    selectedHotbar_ = 0;
}

void Inventory::selectHotbar(std::size_t index) noexcept {
    if (index < hotbarSize) selectedHotbar_ = index;
}

} // namespace rf::game::inventory
