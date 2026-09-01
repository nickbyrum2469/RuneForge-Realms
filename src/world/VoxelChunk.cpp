#include "world/VoxelChunk.h"

#include <algorithm>

namespace rf::world {

VoxelChunk::VoxelChunk() { fill(BlockId::Air); }

BlockId VoxelChunk::get(int x, int y, int z) const noexcept {
    if (!inBounds(x, y, z)) return BlockId::Air;
    return blocks_[index(x, y, z)];
}

void VoxelChunk::set(int x, int y, int z, BlockId block) noexcept {
    if (!inBounds(x, y, z)) return;
    blocks_[index(x, y, z)] = block;
}

void VoxelChunk::fill(BlockId block) noexcept { blocks_.fill(block); }

std::size_t VoxelChunk::solidBlockCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(blocks_.begin(), blocks_.end(), [](BlockId block) {
        return isSolid(block);
    }));
}

} // namespace rf::world
