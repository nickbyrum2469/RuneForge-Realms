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
    const float broad = std::sin(fx * 0.075f) * 1.65f + std::cos(fz * 0.064f) * 1.45f;
    const float detail = std::sin((fx + fz) * 0.12f) * 0.75f + std::cos((fx - fz) * 0.095f) * 0.55f;
    const float random = (static_cast<float>(hash2(worldX / 3, worldZ / 3, seed) & 255u) / 255.0f - 0.5f) * 0.8f;
    return std::clamp(5 + static_cast<int>(std::round(broad + detail + random)), 2, 10);
}

bool TerrainGenerator::treeRootAt(std::uint32_t seed, int worldX, int worldZ) noexcept {
    const std::uint32_t h = hash2(worldX, worldZ, seed ^ 0x51a7f00du);
    if ((h % 181u) != 0u) return false;
    const int ground = surfaceHeight(seed, worldX, worldZ);
    return ground >= 2 && ground <= 9;
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
            for (int y = 0; y <= height; ++y) {
                BlockId block = BlockId::Stone;
                if (y == height) block = BlockId::Grass;
                else if (y >= height - 2) block = BlockId::Dirt;
                chunk.set(localX, y, localZ, block);
            }
        }
    }

    // Roots just outside this chunk can place canopy blocks across the boundary. Sampling a
    // small halo keeps trees deterministic and seamless no matter which chunk loads first.
    for (int rootZ = baseZ - 2; rootZ <= baseZ + VoxelChunk::sizeZ + 1; ++rootZ) {
        for (int rootX = baseX - 2; rootX <= baseX + VoxelChunk::sizeX + 1; ++rootX) {
            if (!treeRootAt(seed, rootX, rootZ)) continue;
            const std::uint32_t h = hash2(rootX, rootZ, seed ^ 0x51a7f00du);
            const int ground = surfaceHeight(seed, rootX, rootZ);
            const int trunkHeight = 3 + static_cast<int>((h >> 8) % 2u);

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
