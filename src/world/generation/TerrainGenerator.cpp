#include "world/generation/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

std::uint32_t hash2(int x, int z, std::uint32_t seed) noexcept {
    const auto ux = static_cast<std::uint32_t>(x) * 0x9e3779b9u;
    const auto uz = static_cast<std::uint32_t>(z) * 0x85ebca6bu;
    return mix(ux ^ uz ^ seed);
}

bool targetInsideChunk(ChunkCoord coord, int worldX, int worldZ) noexcept {
    return chunkFromBlock(worldX, worldZ) == coord;
}

void setGlobal(VoxelChunk& chunk, ChunkCoord coord, int worldX, int y, int worldZ, BlockId block) {
    if (y < 0 || y >= VoxelChunk::sizeY || !targetInsideChunk(coord, worldX, worldZ)) return;
    chunk.set(localBlockX(worldX), y, localBlockZ(worldZ), block);
}

} // namespace

int TerrainGenerator::surfaceHeight(std::uint32_t seed, int worldX, int worldZ) noexcept {
    const float fx = static_cast<float>(worldX) + static_cast<float>(seed % 997u) * 0.13f;
    const float fz = static_cast<float>(worldZ) - static_cast<float>(seed % 571u) * 0.17f;
    // Larger continental waves make water bodies coherent instead of isolated blue holes.
    const float continental = std::sin(fx * 0.022f) * 1.35f + std::cos(fz * 0.019f) * 1.20f;
    const float broad = std::sin(fx * 0.067f) * 1.45f + std::cos(fz * 0.058f) * 1.25f;
    const float detail = std::sin((fx + fz) * 0.115f) * 0.62f + std::cos((fx - fz) * 0.091f) * 0.48f;
    const float random = (static_cast<float>(hash2(worldX / 4, worldZ / 4, seed) & 255u) / 255.0f - 0.5f) * 0.48f;
    return std::clamp(5 + static_cast<int>(std::round(continental + broad + detail + random)), 2, 11);
}

bool TerrainGenerator::treeRootAt(std::uint32_t seed, int worldX, int worldZ) noexcept {
    const std::uint32_t h = hash2(worldX, worldZ, seed ^ 0x51a7f00du);
    if ((h % 197u) != 0u) return false;
    const int ground = surfaceHeight(seed, worldX, worldZ);
    return ground >= waterLevel + 1 && ground <= 10;
}

VoxelChunk TerrainGenerator::generateChunk(std::uint32_t seed, ChunkCoord coord) {
    VoxelChunk chunk;
    const int baseX = coord.x * VoxelChunk::sizeX;
    const int baseZ = coord.z * VoxelChunk::sizeZ;

    for (int localZ = 0; localZ < VoxelChunk::sizeZ; ++localZ) {
        for (int localX = 0; localX < VoxelChunk::sizeX; ++localX) {
            const int worldX = baseX + localX;
            const int worldZ = baseZ + localZ;
            const int height = surfaceHeight(seed, worldX, worldZ);
            const bool submerged = height < waterLevel;
            for (int y = 0; y <= height; ++y) {
                BlockId block = BlockId::Stone;
                if (y >= height - 2) block = submerged ? BlockId::Dirt : (y == height ? BlockId::Grass : BlockId::Dirt);
                chunk.set(localX, y, localZ, block);
            }
            if (submerged) {
                for (int y = height + 1; y <= waterLevel && y < VoxelChunk::sizeY; ++y) {
                    chunk.set(localX, y, localZ, BlockId::Water);
                }
            }
        }
    }

    // Roots just outside this chunk can place canopy blocks across the boundary. Sampling a small
    // halo keeps trees deterministic and seamless no matter which chunk loads first.
    for (int rootZ = baseZ - 2; rootZ <= baseZ + VoxelChunk::sizeZ + 1; ++rootZ) {
        for (int rootX = baseX - 2; rootX <= baseX + VoxelChunk::sizeX + 1; ++rootX) {
            if (!treeRootAt(seed, rootX, rootZ)) continue;
            const std::uint32_t h = hash2(rootX, rootZ, seed ^ 0x51a7f00du);
            const int ground = surfaceHeight(seed, rootX, rootZ);
            const int trunkHeight = 3 + static_cast<int>((h >> 8) % 3u);

            for (int y = ground + 1; y <= ground + trunkHeight; ++y) {
                setGlobal(chunk, coord, rootX, y, rootZ, BlockId::Wood);
            }

            const int canopy = ground + trunkHeight;
            for (int dy = -1; dy <= 2; ++dy) {
                for (int dz = -2; dz <= 2; ++dz) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int spread = std::abs(dx) + std::abs(dz) + std::max(std::abs(dy) - 1, 0);
                        if (spread > 4) continue;
                        const int bx = rootX + dx;
                        const int by = canopy + dy;
                        const int bz = rootZ + dz;
                        if (!targetInsideChunk(coord, bx, bz) || by < 0 || by >= VoxelChunk::sizeY) continue;
                        const int lx = localBlockX(bx);
                        const int lz = localBlockZ(bz);
                        if (chunk.get(lx, by, lz) == BlockId::Air) chunk.set(lx, by, lz, BlockId::Leaves);
                    }
                }
            }
        }
    }

    return chunk;
}

} // namespace rf::world::generation
