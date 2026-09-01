#pragma once

#include "world/Block.h"
#include "world/micro/MicroVoxelState.h"

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
    micro::MicroCoord micro{};
    float worldX{};
    float worldY{};
    float worldZ{};
    bool microResolved{false};
};

} // namespace rf::world
