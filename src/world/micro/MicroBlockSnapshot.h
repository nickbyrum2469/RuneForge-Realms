#pragma once

#include "world/Block.h"
#include "world/micro/MicroVoxelState.h"

namespace rf::world::micro {

struct MicroBlockSnapshot {
    int localX{};
    int y{};
    int localZ{};
    BlockId block{BlockId::Stone};
    MicroVoxelState state{};
    bool owned{true}; // false means neighbor halo used only for visibility/occupancy queries.
};

} // namespace rf::world::micro
