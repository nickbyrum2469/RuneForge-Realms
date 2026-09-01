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
};

} // namespace rf::world::micro
