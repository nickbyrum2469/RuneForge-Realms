#include "world/FrontierWorld.h"

#include "world/generation/TerrainGenerator.h"

#include <cmath>

namespace rf::world {

void FrontierWorld::generate(std::uint32_t seed) {
    seed_ = seed;
    chunks_.clear();
    edits_.clear();
    streamCenter_ = {0, 0};
    (void)chunks_.update(streamCenter_, initialChunkRadius, initialChunkRadius + 1,
                         [this](ChunkCoord coord) { return generateChunk(coord); });
}

bool FrontierWorld::updateStreaming(float worldX, float worldZ) {
    const ChunkCoord center = chunkFromWorld(worldX, worldZ);
    const auto delta = chunks_.update(center, streamingLoadRadius, streamingRetainRadius,
                                      [this](ChunkCoord coord) { return generateChunk(coord); });
    streamCenter_ = center;
    return delta.changed();
}

VoxelChunk FrontierWorld::generateChunk(ChunkCoord coord) const {
    VoxelChunk chunk = generation::TerrainGenerator::generateChunk(seed_, coord);
    applyStoredEditsToChunk(coord, chunk);
    return chunk;
}

void FrontierWorld::applyStoredEditsToChunk(ChunkCoord coord, VoxelChunk& chunk) const {
    for (const auto& [position, block] : edits_) {
        if (chunkFromBlock(position.x, position.z) != coord) continue;
        if (position.y < 0 || position.y >= VoxelChunk::sizeY) continue;
        chunk.set(localBlockX(position.x), position.y, localBlockZ(position.z), block);
    }
}

BlockId FrontierWorld::getBlock(int x, int y, int z) const noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    const VoxelChunk* chunk = chunks_.find(chunkFromBlock(x, z));
    if (!chunk) return BlockId::Air;
    return chunk->get(localBlockX(x), y, localBlockZ(z));
}

bool FrontierWorld::setBlock(int x, int y, int z, BlockId block, bool recordEdit) {
    if (y < 0 || y >= VoxelChunk::sizeY) return false;
    const BlockCoord position{x, y, z};
    const ChunkCoord coord = chunkFromBlock(x, z);

    if (recordEdit) edits_[position] = block;

    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return recordEdit;

    chunk->set(localBlockX(x), y, localBlockZ(z), block);
    chunks_.markDirty(coord);
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
    dx /= len;
    dy /= len;
    dz /= len;

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

VoxelMesh FrontierWorld::buildChunkMesh(ChunkCoord coord) const {
    const VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return {};
    VoxelMesh mesh = GreedyMesher::build(*chunk);
    if (!mesh.empty()) {
        VoxelMesh translated;
        translated.append(mesh, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                          static_cast<float>(coord.z * VoxelChunk::sizeZ));
        return translated;
    }
    return mesh;
}

VoxelMesh FrontierWorld::buildMesh() const {
    VoxelMesh result;
    for (const ChunkCoord coord : chunks_.loadedCoords()) {
        const VoxelChunk* chunk = chunks_.find(coord);
        if (!chunk) continue;
        const VoxelMesh mesh = GreedyMesher::build(*chunk);
        result.append(mesh, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                      static_cast<float>(coord.z * VoxelChunk::sizeZ));
    }
    return result;
}

std::size_t FrontierWorld::solidBlockCount() const noexcept {
    std::size_t result = 0;
    for (const ChunkCoord coord : chunks_.loadedCoords()) {
        const VoxelChunk* chunk = chunks_.find(coord);
        if (chunk) result += chunk->solidBlockCount();
    }
    return result;
}

void FrontierWorld::markChunkMeshesReady() {
    for (const ChunkCoord coord : chunks_.dirtyCoords()) chunks_.markReady(coord);
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
