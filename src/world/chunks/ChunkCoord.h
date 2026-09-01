#pragma once

#include "world/VoxelChunk.h"

#include <algorithm>
#include <cmath>
#include <compare>

namespace rf::world {

struct ChunkCoord {
    int x{};
    int z{};
    auto operator<=>(const ChunkCoord&) const = default;
};

[[nodiscard]] inline int floorDiv(int value, int divisor) noexcept {
    int q = value / divisor;
    const int r = value % divisor;
    if (r != 0 && ((r < 0) != (divisor < 0))) --q;
    return q;
}

[[nodiscard]] inline int floorMod(int value, int divisor) noexcept {
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

[[nodiscard]] inline ChunkCoord chunkFromBlock(int x, int z) noexcept {
    return {floorDiv(x, VoxelChunk::sizeX), floorDiv(z, VoxelChunk::sizeZ)};
}

[[nodiscard]] inline ChunkCoord chunkFromWorld(float x, float z) noexcept {
    return chunkFromBlock(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(z)));
}

[[nodiscard]] inline int localBlockX(int worldX) noexcept {
    return floorMod(worldX, VoxelChunk::sizeX);
}

[[nodiscard]] inline int localBlockZ(int worldZ) noexcept {
    return floorMod(worldZ, VoxelChunk::sizeZ);
}

[[nodiscard]] inline int chebyshevDistance(ChunkCoord a, ChunkCoord b) noexcept {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

} // namespace rf::world
