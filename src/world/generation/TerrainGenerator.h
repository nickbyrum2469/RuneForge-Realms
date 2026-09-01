#pragma once

#include "world/VoxelChunk.h"
#include "world/chunks/ChunkCoord.h"

#include <cstdint>

namespace rf::world::generation {

class TerrainGenerator {
public:
    [[nodiscard]] static VoxelChunk generateChunk(std::uint32_t seed, ChunkCoord coord);
    [[nodiscard]] static int surfaceHeight(std::uint32_t seed, int worldX, int worldZ) noexcept;
    [[nodiscard]] static bool treeRootAt(std::uint32_t seed, int worldX, int worldZ) noexcept;
};

} // namespace rf::world::generation
