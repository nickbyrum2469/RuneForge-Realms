#include "world/FrontierWorld.h"

#include "world/generation/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace rf::world {

void FrontierWorld::generate(std::uint32_t seed, ChunkCoord initialCenter) {
    streamer_.reset();
    seed_ = seed;
    chunks_.clear();
    edits_.clear();
    streamCenter_ = initialCenter;
    (void)chunks_.update(streamCenter_, initialChunkRadius, initialChunkRadius + 1,
                         [this](ChunkCoord coord) { return generateChunk(coord); });
}

void FrontierWorld::requestMissingChunks(ChunkCoord center) {
    std::vector<ChunkCoord> requested;
    requested.reserve(static_cast<std::size_t>((streamingLoadRadius * 2 + 1) *
                                               (streamingLoadRadius * 2 + 1)));
    for (int dz = -streamingLoadRadius; dz <= streamingLoadRadius; ++dz) {
        for (int dx = -streamingLoadRadius; dx <= streamingLoadRadius; ++dx) {
            const ChunkCoord coord{center.x + dx, center.z + dz};
            if (!chunks_.contains(coord) && !streamer_.pending(coord)) requested.push_back(coord);
        }
    }

    std::sort(requested.begin(), requested.end(), [center](ChunkCoord a, ChunkCoord b) {
        const int ad = chebyshevDistance(a, center);
        const int bd = chebyshevDistance(b, center);
        if (ad != bd) return ad < bd;
        const int am = std::abs(a.x - center.x) + std::abs(a.z - center.z);
        const int bm = std::abs(b.x - center.x) + std::abs(b.z - center.z);
        if (am != bm) return am < bm;
        return a < b;
    });

    for (const ChunkCoord coord : requested) (void)streamer_.request(seed_, coord);
}

bool FrontierWorld::updateStreaming(float worldX, float worldZ) {
    const ChunkCoord center = chunkFromWorld(worldX, worldZ);
    bool changed = false;

    for (auto& completed : streamer_.drainCompleted()) {
        if (chebyshevDistance(completed.coord, center) > streamingRetainRadius) continue;
        applyStoredEditsToChunk(completed.coord, completed.voxels);
        chunks_.insert(completed.coord, std::move(completed.voxels), ChunkState::Dirty);
        changed = true;
    }

    const auto evicted = chunks_.evictOutside(center, streamingRetainRadius);
    changed = changed || !evicted.empty();
    streamCenter_ = center;
    requestMissingChunks(center);
    return changed;
}

void FrontierWorld::waitForStreamingIdle() {
    streamer_.waitIdle();
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
        const BlockCoord current{static_cast<int>(std::floor(ox + dx * distance)),
                                 static_cast<int>(std::floor(oy + dy * distance)),
                                 static_cast<int>(std::floor(oz + dz * distance))};
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
    const VoxelMesh local = GreedyMesher::build(*chunk);
    if (local.empty()) return local;
    VoxelMesh translated;
    translated.append(local, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                      static_cast<float>(coord.z * VoxelChunk::sizeZ));
    return translated;
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
