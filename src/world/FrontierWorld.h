#pragma once

#include "world/GreedyMesher.h"
#include "world/WorldEdit.h"
#include "world/chunks/ChunkManager.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace rf::world {

class FrontierWorld {
public:
    void generate(std::uint32_t seed);
    [[nodiscard]] bool updateStreaming(float worldX, float worldZ);

    [[nodiscard]] std::uint32_t seed() const noexcept { return chunks_.seed(); }
    [[nodiscard]] BlockId getBlock(int x, int y, int z) const noexcept;
    bool setBlock(int x, int y, int z, BlockId block, bool recordEdit = true);
    [[nodiscard]] int topSolidY(int x, int z) const noexcept;
    [[nodiscard]] bool collidesAabb(float minX, float minY, float minZ,
                                    float maxX, float maxY, float maxZ) const noexcept;
    [[nodiscard]] RaycastHit raycast(float ox, float oy, float oz,
                                     float dx, float dy, float dz,
                                     float maxDistance) const noexcept;

    [[nodiscard]] VoxelMesh buildMesh() const;
    [[nodiscard]] VoxelMesh buildChunkMesh(ChunkCoord coord) const;
    [[nodiscard]] std::optional<VoxelChunk> chunkSnapshot(ChunkCoord coord) const;
    [[nodiscard]] std::vector<ChunkCoord> loadedChunkCoords() const;
    [[nodiscard]] std::vector<ChunkCoord> takeDirtyChunks();
    [[nodiscard]] std::vector<ChunkCoord> takeUnloadedChunks();
    [[nodiscard]] std::uint64_t chunkRevision(ChunkCoord coord) const noexcept;
    [[nodiscard]] ChunkStreamingStats streamingStats() const noexcept;
    [[nodiscard]] std::size_t solidBlockCount() const noexcept;

    [[nodiscard]] std::vector<BlockEdit> edits() const;
    void applyEdit(const BlockEdit& edit);
    void clearEdits() noexcept { editsByChunk_.clear(); }

private:
    [[nodiscard]] static ChunkCoord coordForBlock(int x, int z) noexcept;
    void applyEditsToChunk(ChunkCoord coord);
    void markMeshNeighborhoodDirty(ChunkCoord coord, int localX, int localZ) noexcept;

    ChunkManager chunks_;
    std::map<ChunkCoord, std::map<BlockCoord, BlockId>> editsByChunk_;
    std::vector<ChunkCoord> recentlyUnloaded_;
};

} // namespace rf::world
