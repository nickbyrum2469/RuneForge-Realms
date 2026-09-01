#pragma once

#include "world/VoxelChunk.h"
#include "world/chunks/ChunkCoord.h"

#include <cstdint>

namespace rf::world::generation {

class TerrainGenerator {
public:
    [[nodiscard]] static VoxelChunk generateChunk(std::uint32_t seed, ChunkCoord coord);
    [[nodiscard]] static int terrainHeight(std::uint32_t seed, int worldX, int worldZ) noexcept;

private:
    [[nodiscard]] static std::uint32_t hash2(int x, int z, std::uint32_t seed) noexcept;
};

} // namespace rf::world::generation
