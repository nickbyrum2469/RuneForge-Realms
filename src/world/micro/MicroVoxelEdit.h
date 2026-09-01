#pragma once

#include "world/Block.h"
#include "world/WorldEdit.h"
#include "world/micro/MicroVoxelState.h"

#include <array>
#include <cstdint>

namespace rf::world::micro {

struct MicroVoxelEdit {
    BlockCoord position{};
    BlockId block{BlockId::Stone};
    std::array<std::uint64_t, 8> occupancyWords{};
};

[[nodiscard]] MicroVoxelEdit makeEdit(BlockCoord position, BlockId block,
                                      const MicroVoxelState& state) noexcept;
[[nodiscard]] MicroVoxelState stateFromEdit(const MicroVoxelEdit& edit) noexcept;

} // namespace rf::world::micro
