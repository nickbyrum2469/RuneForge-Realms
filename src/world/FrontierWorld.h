#pragma once

#include "world/GreedyMesher.h"
#include "world/WorldEdit.h"
#include "world/chunks/ChunkManager.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace rf::world {

class FrontierWorld {
public:
    static constexpr int initialChunkRadius = 3;
    static constexpr int streamingResidentRadius = 2;
    static constexpr int streamingPrefetchRadius = 5;
    static constexpr int streamingRetainRadius = 7;

    void generate(std::uint32_t seed);
    [[nodiscard]] bool updateStreaming(float worldX, float worldZ);

    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }
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
    [[nodiscard]] std::size_t solidBlockCount() const noexcept;
    [[nodiscard]] std::size_t loadedChunkCount() const noexcept { return chunks_.loadedCount(); }
    [[nodiscard]] std::vector<ChunkCoord> loadedChunkCoords() const { return chunks_.loadedCoords(); }
    [[nodiscard]] std::vector<ChunkCoord> takeDirtyChunkCoords();
    [[nodiscard]] std::vector<ChunkCoord> takeUnloadedChunkCoords();
    [[nodiscard]] std::uint64_t chunkRevision(ChunkCoord coord) const noexcept { return chunks_.revision(coord); }
    [[nodiscard]] ChunkStreamingStats streamingStats() const noexcept { return chunks_.stats(); }

    [[nodiscard]] std::vector<BlockEdit> edits() const;
    void applyEdit(const BlockEdit& edit);
    void clearEdits() noexcept { editsByChunk_.clear(); }

private:
    [[nodiscard]] VoxelChunk generateRawChunk(ChunkCoord coord) const;
    void applyStoredEditsToChunk(ChunkCoord coord);
    void markMeshNeighborhoodDirty(ChunkCoord coord, int localX, int localZ) noexcept;

    std::uint32_t seed_{1337};
    ChunkCoord streamCenter_{};
    ChunkManager chunks_;
    std::map<ChunkCoord, std::map<BlockCoord, BlockId>> editsByChunk_;
    std::vector<ChunkCoord> recentlyUnloaded_;
};

} // namespace rf::world
