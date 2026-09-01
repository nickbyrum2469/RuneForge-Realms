#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>

namespace rf::world {

ChunkCoord FrontierWorld::coordForBlock(int x, int z) noexcept {
    return {floorDiv(x, VoxelChunk::sizeX), floorDiv(z, VoxelChunk::sizeZ)};
}

void FrontierWorld::generate(std::uint32_t seed) {
    chunks_.reset(seed);
    editsByChunk_.clear();
    recentlyUnloaded_.clear();
    updateStreaming(0.0f, 0.0f);
}

bool FrontierWorld::updateStreaming(float worldX, float worldZ) {
    const int blockX = static_cast<int>(std::floor(worldX));
    const int blockZ = static_cast<int>(std::floor(worldZ));
    const ChunkStreamUpdate update = chunks_.update(blockX, blockZ);
    for (const ChunkCoord coord : update.loaded) {
        applyEditsToChunk(coord);
        chunks_.markDirty(coord);
    }
    recentlyUnloaded_.insert(recentlyUnloaded_.end(), update.unloaded.begin(), update.unloaded.end());
    return update.geometryChanged();
}

void FrontierWorld::applyEditsToChunk(ChunkCoord coord) {
    auto* chunk = chunks_.chunkAt(coord);
    if (!chunk) return;
    const auto edits = editsByChunk_.find(coord);
    if (edits == editsByChunk_.end()) return;
    for (const auto& [position, block] : edits->second) {
        chunk->set(floorMod(position.x, VoxelChunk::sizeX), position.y,
                   floorMod(position.z, VoxelChunk::sizeZ), block);
    }
}

BlockId FrontierWorld::getBlock(int x, int y, int z) const noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    const ChunkCoord coord = coordForBlock(x, z);
    const VoxelChunk* chunk = chunks_.chunkAt(coord);
    if (!chunk) return BlockId::Air;
    return chunk->get(floorMod(x, VoxelChunk::sizeX), y, floorMod(z, VoxelChunk::sizeZ));
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
    const ChunkCoord coord = coordForBlock(x, z);
    std::vector<ChunkCoord> loaded;
    chunks_.ensureLoaded(coord, &loaded);
    for (const ChunkCoord item : loaded) applyEditsToChunk(item);

    VoxelChunk* chunk = chunks_.chunkAt(coord);
    if (!chunk) return false;
    const int localX = floorMod(x, VoxelChunk::sizeX);
    const int localZ = floorMod(z, VoxelChunk::sizeZ);
    if (chunk->get(localX, y, localZ) == block) return false;
    chunk->set(localX, y, localZ, block);
    if (recordEdit) editsByChunk_[coord][{x, y, z}] = block;
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
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length <= 0.00001f) return {};
    dx /= length;
    dy /= length;
    dz /= length;

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
    VoxelMesh result;
    const auto snapshot = chunkSnapshot(coord);
    if (!snapshot) return result;
    const VoxelMesh local = GreedyMesher::build(*snapshot);
    result.append(local, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                  static_cast<float>(coord.z * VoxelChunk::sizeZ));
    return result;
}

VoxelMesh FrontierWorld::buildMesh() const {
    VoxelMesh result;
    for (const ChunkCoord coord : loadedChunkCoords()) {
        const VoxelMesh mesh = buildChunkMesh(coord);
        result.append(mesh, 0.0f, 0.0f, 0.0f);
    }
    return result;
}

std::optional<VoxelChunk> FrontierWorld::chunkSnapshot(ChunkCoord coord) const {
    const VoxelChunk* chunk = chunks_.chunkAt(coord);
    if (!chunk) return std::nullopt;
    return *chunk;
}

std::vector<ChunkCoord> FrontierWorld::loadedChunkCoords() const {
    return chunks_.loadedCoords();
}

std::vector<ChunkCoord> FrontierWorld::takeDirtyChunks() {
    return chunks_.takeDirtyChunks();
}

std::vector<ChunkCoord> FrontierWorld::takeUnloadedChunks() {
    std::vector<ChunkCoord> result = std::move(recentlyUnloaded_);
    recentlyUnloaded_.clear();
    return result;
}

std::uint64_t FrontierWorld::chunkRevision(ChunkCoord coord) const noexcept {
    return chunks_.revision(coord);
}

ChunkStreamingStats FrontierWorld::streamingStats() const noexcept {
    return chunks_.stats();
}

std::size_t FrontierWorld::solidBlockCount() const noexcept {
    std::size_t result = 0;
    for (const ChunkCoord coord : chunks_.loadedCoords()) {
        if (const VoxelChunk* chunk = chunks_.chunkAt(coord)) result += chunk->solidBlockCount();
    }
    return result;
}

std::vector<BlockEdit> FrontierWorld::edits() const {
    std::vector<BlockEdit> result;
    std::size_t count = 0;
    for (const auto& [coord, chunkEdits] : editsByChunk_) {
        (void)coord;
        count += chunkEdits.size();
    }
    result.reserve(count);
    for (const auto& [coord, chunkEdits] : editsByChunk_) {
        (void)coord;
        for (const auto& [position, block] : chunkEdits) result.push_back({position, block});
    }
    return result;
}

void FrontierWorld::applyEdit(const BlockEdit& edit) {
    if (edit.position.y < 0 || edit.position.y >= VoxelChunk::sizeY) return;
    const ChunkCoord coord = coordForBlock(edit.position.x, edit.position.z);
    editsByChunk_[coord][edit.position] = edit.block;
    if (VoxelChunk* chunk = chunks_.chunkAt(coord)) {
        const int localX = floorMod(edit.position.x, VoxelChunk::sizeX);
        const int localZ = floorMod(edit.position.z, VoxelChunk::sizeZ);
        chunk->set(localX, edit.position.y, localZ, edit.block);
        markMeshNeighborhoodDirty(coord, localX, localZ);
    }
}

} // namespace rf::world
