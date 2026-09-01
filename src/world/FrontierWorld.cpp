#include "world/FrontierWorld.h"

#include "world/generation/TerrainGenerator.h"

#include <cmath>

namespace rf::world {

void FrontierWorld::generate(std::uint32_t seed) {
    seed_ = seed;
    chunks_.clear();
    editsByChunk_.clear();
    recentlyUnloaded_.clear();
    streamCenter_ = {0, 0};

    const auto generator = [seed](ChunkCoord coord) {
        return generation::TerrainGenerator::generateChunk(seed, coord);
    };
    const auto delta = chunks_.update(streamCenter_, initialChunkRadius, initialChunkRadius,
                                      initialChunkRadius + 1, generator);
    for (const ChunkCoord coord : delta.loaded) {
        applyStoredEditsToChunk(coord);
        markAdjacentChunksDirty(coord);
    }
}

bool FrontierWorld::updateStreaming(float worldX, float worldZ) {
    const ChunkCoord center = chunkFromWorld(worldX, worldZ);
    const std::uint32_t capturedSeed = seed_;
    const auto generator = [capturedSeed](ChunkCoord coord) {
        return generation::TerrainGenerator::generateChunk(capturedSeed, coord);
    };
    const auto delta = chunks_.update(center, streamingResidentRadius, streamingPrefetchRadius,
                                      streamingRetainRadius, generator);
    streamCenter_ = center;

    for (const ChunkCoord coord : delta.loaded) {
        applyStoredEditsToChunk(coord);
        markAdjacentChunksDirty(coord);
    }
    for (const ChunkCoord coord : delta.unloaded) markAdjacentChunksDirty(coord);

    recentlyUnloaded_.insert(recentlyUnloaded_.end(), delta.unloaded.begin(), delta.unloaded.end());
    return delta.changed();
}

void FrontierWorld::applyStoredEditsToChunk(ChunkCoord coord) {
    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return;
    const auto stored = editsByChunk_.find(coord);
    if (stored == editsByChunk_.end()) return;
    for (const auto& [position, block] : stored->second) {
        if (position.y < 0 || position.y >= VoxelChunk::sizeY) continue;
        chunk->set(localBlockX(position.x), position.y, localBlockZ(position.z), block);
    }
}

BlockId FrontierWorld::getBlock(int x, int y, int z) const noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    const VoxelChunk* chunk = chunks_.find(chunkFromBlock(x, z));
    if (!chunk) return BlockId::Air;
    return chunk->get(localBlockX(x), y, localBlockZ(z));
}

void FrontierWorld::markAdjacentChunksDirty(ChunkCoord coord) noexcept {
    chunks_.markDirty({coord.x - 1, coord.z});
    chunks_.markDirty({coord.x + 1, coord.z});
    chunks_.markDirty({coord.x, coord.z - 1});
    chunks_.markDirty({coord.x, coord.z + 1});
}

void FrontierWorld::markMeshNeighborhoodDirty(ChunkCoord coord, int localX, int localZ) noexcept {
    chunks_.markDirty(coord);
    if (localX == 0) chunks_.markDirty({coord.x - 1, coord.z});
    if (localX == VoxelChunk::sizeX - 1) chunks_.markDirty({coord.x + 1, coord.z});
    if (localZ == 0) chunks_.markDirty({coord.x, coord.z - 1});
    if (localZ == VoxelChunk::sizeZ - 1) chunks_.markDirty({coord.x, coord.z + 1});
}

bool FrontierWorld::setBlock(int x, int y, int z, BlockId block, bool recordEdit) {
    if (y < 0 || y >= VoxelChunk::sizeY) return false;
    const BlockCoord position{x, y, z};
    const ChunkCoord coord = chunkFromBlock(x, z);
    if (recordEdit) editsByChunk_[coord][position] = block;

    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return recordEdit;

    const int localX = localBlockX(x);
    const int localZ = localBlockZ(z);
    if (chunk->get(localX, y, localZ) == block) return false;
    chunk->set(localX, y, localZ, block);
    markMeshNeighborhoodDirty(coord, localX, localZ);
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

std::optional<ChunkMeshingSnapshot> FrontierWorld::chunkMeshingSnapshot(ChunkCoord coord) const {
    const VoxelChunk* center = chunks_.find(coord);
    if (!center) return std::nullopt;

    ChunkMeshingSnapshot snapshot;
    snapshot.center = *center;
    if (const VoxelChunk* chunk = chunks_.find({coord.x - 1, coord.z})) snapshot.negativeX = *chunk;
    if (const VoxelChunk* chunk = chunks_.find({coord.x + 1, coord.z})) snapshot.positiveX = *chunk;
    if (const VoxelChunk* chunk = chunks_.find({coord.x, coord.z - 1})) snapshot.negativeZ = *chunk;
    if (const VoxelChunk* chunk = chunks_.find({coord.x, coord.z + 1})) snapshot.positiveZ = *chunk;
    return snapshot;
}

VoxelMesh FrontierWorld::buildChunkMesh(ChunkCoord coord) const {
    const auto snapshot = chunkMeshingSnapshot(coord);
    if (!snapshot) return {};
    VoxelMesh local = GreedyMesher::build(*snapshot);
    VoxelMesh translated;
    translated.append(local, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                      static_cast<float>(coord.z * VoxelChunk::sizeZ));
    return translated;
}

VoxelMesh FrontierWorld::buildMesh() const {
    VoxelMesh result;
    for (const ChunkCoord coord : chunks_.loadedCoords()) {
        const VoxelMesh mesh = buildChunkMesh(coord);
        result.append(mesh, 0.0f, 0.0f, 0.0f);
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

std::vector<ChunkCoord> FrontierWorld::takeUnloadedChunkCoords() {
    std::vector<ChunkCoord> result = std::move(recentlyUnloaded_);
    recentlyUnloaded_.clear();
    return result;
}

std::vector<BlockEdit> FrontierWorld::edits() const {
    std::vector<BlockEdit> result;
    std::size_t total = 0;
    for (const auto& [coord, chunkEdits] : editsByChunk_) {
        (void)coord;
        total += chunkEdits.size();
    }
    result.reserve(total);
    for (const auto& [coord, chunkEdits] : editsByChunk_) {
        (void)coord;
        for (const auto& [position, block] : chunkEdits) result.push_back({position, block});
    }
    return result;
}

void FrontierWorld::applyEdit(const BlockEdit& edit) {
    if (edit.position.y < 0 || edit.position.y >= VoxelChunk::sizeY) return;
    const ChunkCoord coord = chunkFromBlock(edit.position.x, edit.position.z);
    editsByChunk_[coord][edit.position] = edit.block;
    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return;
    const int localX = localBlockX(edit.position.x);
    const int localZ = localBlockZ(edit.position.z);
    chunk->set(localX, edit.position.y, localZ, edit.block);
    markMeshNeighborhoodDirty(coord, localX, localZ);
}

} // namespace rf::world
