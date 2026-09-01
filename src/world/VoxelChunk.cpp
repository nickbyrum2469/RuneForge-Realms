#include "world/VoxelChunk.h"

#include <algorithm>
#include <cmath>

namespace rf::world {

VoxelChunk::VoxelChunk() {
    fill(BlockId::Air);
}

BlockId VoxelChunk::get(int x, int y, int z) const noexcept {
    if (!inBounds(x, y, z)) return BlockId::Air;
    return blocks_[index(x, y, z)];
}

void VoxelChunk::set(int x, int y, int z, BlockId block) noexcept {
    if (!inBounds(x, y, z)) return;
    blocks_[index(x, y, z)] = block;
}

void VoxelChunk::fill(BlockId block) noexcept {
    blocks_.fill(block);
}

std::size_t VoxelChunk::solidBlockCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(blocks_.begin(), blocks_.end(), [](BlockId block) {
        return isSolid(block);
    }));
}

VoxelChunk VoxelChunk::makeDemoTerrain() {
    VoxelChunk chunk;

    // A deterministic, gently rolling chunk. This is intentionally tiny, but it is real
    // block state now: the renderer sees only a mesh generated from these 4,096 cells.
    for (int z = 0; z < sizeZ; ++z) {
        for (int x = 0; x < sizeX; ++x) {
            const float wave = std::sin(static_cast<float>(x) * 0.48f) * 1.15f +
                               std::cos(static_cast<float>(z) * 0.37f) * 0.95f +
                               std::sin(static_cast<float>(x + z) * 0.21f) * 0.55f;
            const int height = std::clamp(5 + static_cast<int>(std::round(wave)), 3, 8);

            for (int y = 0; y <= height; ++y) {
                BlockId block = BlockId::Stone;
                if (y == height) block = BlockId::Grass;
                else if (y >= height - 2) block = BlockId::Dirt;
                chunk.set(x, y, z, block);
            }
        }
    }

    // A small exposed stone outcrop demonstrates that separate materials survive greedy merging.
    for (int z = 2; z <= 4; ++z) {
        for (int x = 11; x <= 13; ++x) {
            const int baseY = 7;
            chunk.set(x, baseY, z, BlockId::Stone);
            if ((x + z) % 2 == 0) chunk.set(x, baseY + 1, z, BlockId::Stone);
        }
    }

    // Block tree. This will later become a semantic tree asset with richer micro-detail, but today
    // it proves that world block state, not shader-side fake instances, drives the geometry.
    constexpr int trunkX = 7;
    constexpr int trunkZ = 8;
    int groundY = 0;
    for (int y = sizeY - 1; y >= 0; --y) {
        if (isSolid(chunk.get(trunkX, y, trunkZ))) {
            groundY = y;
            break;
        }
    }

    for (int y = groundY + 1; y <= groundY + 4 && y < sizeY; ++y) {
        chunk.set(trunkX, y, trunkZ, BlockId::Wood);
    }

    const int canopyY = std::min(groundY + 5, sizeY - 2);
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                const int taxicab = std::abs(dx) + std::abs(dz) + std::abs(dy);
                if (taxicab > 4) continue;
                const int x = trunkX + dx;
                const int y = canopyY + dy;
                const int z = trunkZ + dz;
                if (chunk.get(x, y, z) == BlockId::Air) chunk.set(x, y, z, BlockId::Leaves);
            }
        }
    }

    if (canopyY + 2 < sizeY) chunk.set(trunkX, canopyY + 2, trunkZ, BlockId::Leaves);
    return chunk;
}

SurfaceMaterial surfaceMaterial(BlockId block, int axis, int normalSign) noexcept {
    switch (block) {
        case BlockId::Grass:
            // Upward grass is green; the vertical/bottom surfaces reveal soil.
            return axis == 1 && normalSign > 0 ? SurfaceMaterial::Grass : SurfaceMaterial::Dirt;
        case BlockId::Dirt: return SurfaceMaterial::Dirt;
        case BlockId::Stone: return SurfaceMaterial::Stone;
        case BlockId::Wood: return SurfaceMaterial::Wood;
        case BlockId::Leaves: return SurfaceMaterial::Leaves;
        case BlockId::Air: break;
    }
    return SurfaceMaterial::Dirt;
}

} // namespace rf::world
