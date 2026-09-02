#pragma once

#include "world/Block.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace rf::game::items {

enum class ItemId : std::uint16_t {
    None = 0,
    GrassBlock,
    DirtBlock,
    StoneBlock,
    OakLog,
    Leaves,
};

[[nodiscard]] constexpr std::string_view itemName(ItemId id) noexcept {
    switch (id) {
        case ItemId::GrassBlock: return "Grass Block";
        case ItemId::DirtBlock: return "Dirt Block";
        case ItemId::StoneBlock: return "Stone Block";
        case ItemId::OakLog: return "Oak Log";
        case ItemId::Leaves: return "Leaves";
        case ItemId::None: break;
    }
    return "Empty";
}

[[nodiscard]] constexpr std::optional<ItemId> itemForBlock(world::BlockId block) noexcept {
    switch (block) {
        case world::BlockId::Grass: return ItemId::GrassBlock;
        case world::BlockId::Dirt: return ItemId::DirtBlock;
        case world::BlockId::Stone: return ItemId::StoneBlock;
        case world::BlockId::Wood: return ItemId::OakLog;
        case world::BlockId::Leaves: return ItemId::Leaves;
        case world::BlockId::Water:
        case world::BlockId::Air: break;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<world::BlockId> blockForItem(ItemId item) noexcept {
    switch (item) {
        case ItemId::GrassBlock: return world::BlockId::Grass;
        case ItemId::DirtBlock: return world::BlockId::Dirt;
        case ItemId::StoneBlock: return world::BlockId::Stone;
        case ItemId::OakLog: return world::BlockId::Wood;
        case ItemId::Leaves: return world::BlockId::Leaves;
        case ItemId::None: break;
    }
    return std::nullopt;
}

} // namespace rf::game::items
