#pragma once

#include "world/Block.h"

#include <compare>

namespace rf::world {

struct BlockCoord {
    int x{};
    int y{};
    int z{};
    auto operator<=>(const BlockCoord&) const = default;
};

struct BlockEdit {
    BlockCoord position{};
    BlockId block{BlockId::Air};
};

struct RaycastHit {
    bool hit{false};
    BlockCoord block{};
    BlockCoord adjacent{};
};

} // namespace rf::world
