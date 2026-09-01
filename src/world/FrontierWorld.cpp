#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rf::world {
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

} // namespace

int FrontierWorld::floorDiv(int value, int divisor) noexcept {
    int q = value / divisor;
    const int r = value % divisor;
    if (r != 0 && ((r < 0) != (divisor < 0))) --q;
    return q;
}

int FrontierWorld::floorMod(int value, int divisor) noexcept {
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

VoxelChunk* FrontierWorld::chunkAt(int chunkX, int chunkZ) noexcept {
    const auto it = chunks_.find({chunkX, chunkZ});
    return it == chunks_.end() ? nullptr : &it->second;
}

const VoxelChunk* FrontierWorld::chunkAt(int chunkX, int chunkZ) const noexcept {
    const auto it = chunks_.find({chunkX, chunkZ});
    return it == chunks_.end() ? nullptr : &it->second;
}

void FrontierWorld::generate(std::uint32_t seed) {
    seed_ = seed;
    chunks_.clear();
    edits_.clear();
    for (int cz = -chunkRadius; cz <= chunkRadius; ++cz) {
        for (int cx = -chunkRadius; cx <= chunkRadius; ++cx) chunks_.emplace(ChunkCoord{cx, cz}, VoxelChunk{});
    }
    generateTerrain();
    generateTrees();
}

void FrontierWorld::generateTerrain() {
    for (int z = worldMin; z <= worldMax; ++z) {
        for (int x = worldMin; x <= worldMax; ++x) {
            const float fx = static_cast<float>(x) + static_cast<float>(seed_ % 997u) * 0.13f;
            const float fz = static_cast<float>(z) - static_cast<float>(seed_ % 571u) * 0.17f;
            const float broad = std::sin(fx * 0.075f) * 1.65f + std::cos(fz * 0.064f) * 1.45f;
            const float detail = std::sin((fx + fz) * 0.12f) * 0.75f + std::cos((fx - fz) * 0.095f) * 0.55f;
            const float random = (static_cast<float>(hash2(x / 3, z / 3, seed_) & 255u) / 255.0f - 0.5f) * 0.8f;
            const int height = std::clamp(5 + static_cast<int>(std::round(broad + detail + random)), 2, 10);

            for (int y = 0; y <= height; ++y) {
                BlockId block = BlockId::Stone;
                if (y == height) block = BlockId::Grass;
                else if (y >= height - 2) block = BlockId::Dirt;
                setBlock(x, y, z, block, false);
            }
        }
    }
}

void FrontierWorld::generateTrees() {
    for (int z = worldMin + 3; z <= worldMax - 3; ++z) {
        for (int x = worldMin + 3; x <= worldMax - 3; ++x) {
            const std::uint32_t h = hash2(x, z, seed_ ^ 0x51a7f00du);
            if ((h % 181u) != 0u) continue;
            const int ground = topSolidY(x, z);
            if (ground < 2 || ground > 9 || getBlock(x, ground, z) != BlockId::Grass) continue;

            const int trunkHeight = 3 + static_cast<int>((h >> 8) % 2u);
            for (int y = ground + 1; y <= ground + trunkHeight; ++y) setBlock(x, y, z, BlockId::Wood, false);

            const int canopy = ground + trunkHeight;
            for (int dy = -1; dy <= 2; ++dy) {
                for (int dz = -2; dz <= 2; ++dz) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int spread = std::abs(dx) + std::abs(dz) + std::max(std::abs(dy) - 1, 0);
                        if (spread > 4) continue;
                        const int bx = x + dx;
                        const int by = canopy + dy;
                        const int bz = z + dz;
                        if (by >= VoxelChunk::sizeY || getBlock(bx, by, bz) != BlockId::Air) continue;
                        setBlock(bx, by, bz, BlockId::Leaves, false);
                    }
                }
            }
        }
    }
}

BlockId FrontierWorld::getBlock(int x, int y, int z) const noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    const int cx = floorDiv(x, VoxelChunk::sizeX);
    const int cz = floorDiv(z, VoxelChunk::sizeZ);
    const VoxelChunk* chunk = chunkAt(cx, cz);
    if (!chunk) return BlockId::Air;
    return chunk->get(floorMod(x, VoxelChunk::sizeX), y, floorMod(z, VoxelChunk::sizeZ));
}

bool FrontierWorld::setBlock(int x, int y, int z, BlockId block, bool recordEdit) {
    if (y < 0 || y >= VoxelChunk::sizeY) return false;
    const int cx = floorDiv(x, VoxelChunk::sizeX);
    const int cz = floorDiv(z, VoxelChunk::sizeZ);
    VoxelChunk* chunk = chunkAt(cx, cz);
    if (!chunk) return false;
    chunk->set(floorMod(x, VoxelChunk::sizeX), y, floorMod(z, VoxelChunk::sizeZ), block);
    if (recordEdit) edits_[{x, y, z}] = block;
    return true;
}

int FrontierWorld::topSolidY(int x, int z) const noexcept {
    for (int y = VoxelChunk::sizeY - 1; y >= 0; --y) {
        if (isSolid(getBlock(x, y, z))) return y;
    }
    return -1;
}

bool FrontierWorld::collidesAabb(float minX, float minY, float minZ,
                                 float maxX, float maxY, float maxZ) const noexcept {
    constexpr float epsilon = 0.001f;
    const int x0 = static_cast<int>(std::floor(minX));
    const int x1 = static_cast<int>(std::floor(maxX - epsilon));
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::floor(maxY - epsilon));
    const int z0 = static_cast<int>(std::floor(minZ));
    const int z1 = static_cast<int>(std::floor(maxZ - epsilon));
    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (isSolid(getBlock(x, y, z))) return true;
            }
        }
    }
    return false;
}

RaycastHit FrontierWorld::raycast(float ox, float oy, float oz,
                                  float dx, float dy, float dz, float maxDistance) const noexcept {
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len <= 0.00001f) return {};
    dx /= len; dy /= len; dz /= len;

    BlockCoord previous{static_cast<int>(std::floor(ox)), static_cast<int>(std::floor(oy)),
                        static_cast<int>(std::floor(oz))};
    constexpr float step = 0.04f;
    for (float distance = 0.0f; distance <= maxDistance; distance += step) {
        const BlockCoord current{
            static_cast<int>(std::floor(ox + dx * distance)),
            static_cast<int>(std::floor(oy + dy * distance)),
            static_cast<int>(std::floor(oz + dz * distance)),
        };
        if (current != previous && isSolid(getBlock(current.x, current.y, current.z))) {
            return {true, current, previous};
        }
        previous = current;
    }
    return {};
}

VoxelMesh FrontierWorld::buildMesh() const {
    VoxelMesh result;
    for (const auto& [coord, chunk] : chunks_) {
        const VoxelMesh mesh = GreedyMesher::build(chunk);
        result.append(mesh, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                      static_cast<float>(coord.z * VoxelChunk::sizeZ));
    }
    return result;
}

std::size_t FrontierWorld::solidBlockCount() const noexcept {
    std::size_t result = 0;
    for (const auto& [coord, chunk] : chunks_) {
        (void)coord;
        result += chunk.solidBlockCount();
    }
    return result;
}

std::vector<BlockEdit> FrontierWorld::edits() const {
    std::vector<BlockEdit> result;
    result.reserve(edits_.size());
    for (const auto& [position, block] : edits_) result.push_back({position, block});
    return result;
}

void FrontierWorld::applyEdit(const BlockEdit& edit) {
    setBlock(edit.position.x, edit.position.y, edit.position.z, edit.block, true);
}

} // namespace rf::world
