#include "world/generation/TerrainGenerator.h"

#include <algorithm>
#include <cmath>

namespace rf::world::generation {

namespace {
std::uint32_t mix(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}
} // namespace

std::uint32_t TerrainGenerator::hash2(int x, int z, std::uint32_t seed) noexcept {
    const auto ux = static_cast<std::uint32_t>(x) * 0x9e3779b9u;
    const auto uz = static_cast<std::uint32_t>(z) * 0x85ebca6bu;
    return mix(ux ^ uz ^ seed);
}

int TerrainGenerator::terrainHeight(std::uint32_t seed, int worldX, int worldZ) noexcept {
    const float fx = static_cast<float>(worldX) + static_cast<float>(seed % 997u) * 0.13f;
    const float fz = static_cast<float>(worldZ) - static_cast<float>(seed % 571u) * 0.17f;
    const float broad = std::sin(fx * 0.075f) * 1.65f + std::cos(fz * 0.064f) * 1.45f;
    const float detail = std::sin((fx + fz) * 0.12f) * 0.75f + std::cos((fx - fz) * 0.095f) * 0.55f;
    const float random = (static_cast<float>(hash2(worldX / 3, worldZ / 3, seed) & 255u) / 255.0f - 0.5f) * 0.8f;
    return std::clamp(5 + static_cast<int>(std::round(broad + detail + random)), 2, 10);
}

VoxelChunk TerrainGenerator::generateChunk(std::uint32_t seed, ChunkCoord coord) {
    VoxelChunk chunk;
    const int originX = coord.x * VoxelChunk::sizeX;
    const int originZ = coord.z * VoxelChunk::sizeZ;

    for (int localZ = 0; localZ < VoxelChunk::sizeZ; ++localZ) {
        for (int localX = 0; localX < VoxelChunk::sizeX; ++localX) {
            const int worldX = originX + localX;
            const int worldZ = originZ + localZ;
            const int height = terrainHeight(seed, worldX, worldZ);
            for (int y = 0; y <= height; ++y) {
                BlockId block = BlockId::Stone;
                if (y == height) block = BlockId::Grass;
                else if (y >= height - 2) block = BlockId::Dirt;
                chunk.set(localX, y, localZ, block);
            }
        }
    }

    // Trees are deliberately rooted away from chunk borders for now. This keeps chunk generation
    // deterministic and embarrassingly parallel; cross-chunk structures get their own feature pass later.
    for (int localZ = 2; localZ < VoxelChunk::sizeZ - 2; ++localZ) {
        for (int localX = 2; localX < VoxelChunk::sizeX - 2; ++localX) {
            const int worldX = originX + localX;
            const int worldZ = originZ + localZ;
            const std::uint32_t h = hash2(worldX, worldZ, seed ^ 0x51a7f00du);
            if ((h % 181u) != 0u) continue;

            const int ground = terrainHeight(seed, worldX, worldZ);
            if (ground < 2 || ground > 9 || chunk.get(localX, ground, localZ) != BlockId::Grass) continue;
            const int trunkHeight = 3 + static_cast<int>((h >> 8) % 2u);
            if (ground + trunkHeight + 2 >= VoxelChunk::sizeY) continue;

            for (int y = ground + 1; y <= ground + trunkHeight; ++y) chunk.set(localX, y, localZ, BlockId::Wood);
            const int canopy = ground + trunkHeight;
            for (int dy = -1; dy <= 2; ++dy) {
                for (int dz = -2; dz <= 2; ++dz) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int spread = std::abs(dx) + std::abs(dz) + std::max(std::abs(dy) - 1, 0);
                        if (spread > 4) continue;
                        const int x = localX + dx;
                        const int y = canopy + dy;
                        const int z = localZ + dz;
                        if (chunk.get(x, y, z) == BlockId::Air) chunk.set(x, y, z, BlockId::Leaves);
                    }
                }
            }
        }
    }

    return chunk;
}

} // namespace rf::world::generation
