#pragma once

#include "world/GreedyMesher.h"
#include "world/WorldEdit.h"
#include "world/chunks/ChunkManager.h"
#include "world/micro/MicroVoxelEdit.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace rf::world {

struct MicroChipResult {
    bool changed{false};
    bool emptied{false};
    std::size_t removedCells{};
    float solidFraction{1.0f};
};

class FrontierWorld {
public:
    static constexpr int initialChunkRadius = 3;
    static constexpr int streamingResidentRadius = 2;
    static constexpr int streamingPrefetchRadius = 5;
    static constexpr int streamingRetainRadius = 7;

    void generate(std::uint32_t seed);
    [[nodiscard]] bool updateStreaming(float worldX, float worldZ);
    [[nodiscard]] bool advanceSimulation(float deltaSeconds);

    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }
    [[nodiscard]] float worldAgeSeconds() const noexcept { return worldAgeSeconds_; }
    void setWorldAgeSeconds(float value) noexcept;

    [[nodiscard]] BlockId getBlock(int x, int y, int z) const noexcept;
    bool setBlock(int x, int y, int z, BlockId block, bool recordEdit = true);
    void markBlockVisualDirty(BlockCoord position) noexcept {
        markMeshNeighborhoodDirty(chunkFromBlock(position.x, position.z), localBlockX(position.x), localBlockZ(position.z));
    }
    [[nodiscard]] MicroChipResult chipBlock(BlockCoord position, float worldHitX, float worldHitY,
                                            float worldHitZ, int radiusCells);
    [[nodiscard]] const micro::MicroVoxelState* microState(BlockCoord position) const noexcept;
    [[nodiscard]] int topSolidY(int x, int z) const noexcept;
    [[nodiscard]] bool collidesAabb(float minX, float minY, float minZ,
                                    float maxX, float maxY, float maxZ) const noexcept;
    [[nodiscard]] RaycastHit raycast(float ox, float oy, float oz,
                                     float dx, float dy, float dz,
                                     float maxDistance) const noexcept;

    [[nodiscard]] VoxelMesh buildMesh() const;
    [[nodiscard]] VoxelMesh buildChunkMesh(ChunkCoord coord) const;
    [[nodiscard]] std::optional<ChunkMeshingSnapshot> chunkMeshingSnapshot(ChunkCoord coord) const;
    [[nodiscard]] std::size_t solidBlockCount() const noexcept;
    [[nodiscard]] std::size_t loadedChunkCount() const noexcept { return chunks_.loadedCount(); }
    [[nodiscard]] std::size_t promotedBlockCount() const noexcept;
    [[nodiscard]] std::vector<ChunkCoord> loadedChunkCoords() const { return chunks_.loadedCoords(); }
    [[nodiscard]] std::vector<ChunkCoord> dirtyChunkCoords() const { return chunks_.dirtyCoords(); }
    void markChunkMeshQueued(ChunkCoord coord) noexcept { chunks_.markReady(coord); }
    [[nodiscard]] std::vector<ChunkCoord> takeUnloadedChunkCoords();
    [[nodiscard]] std::uint64_t chunkRevision(ChunkCoord coord) const noexcept { return chunks_.revision(coord); }
    [[nodiscard]] ChunkStreamingStats streamingStats() const noexcept { return chunks_.stats(); }

    [[nodiscard]] std::vector<BlockEdit> edits() const;
    void applyEdit(const BlockEdit& edit);
    [[nodiscard]] std::vector<micro::MicroVoxelEdit> microEdits() const;
    void applyMicroEdit(const micro::MicroVoxelEdit& edit);
    void clearEdits() noexcept { editsByChunk_.clear(); microByChunk_.clear(); }

private:
    struct PromotedBlock {
        BlockId block{BlockId::Stone};
        micro::MicroVoxelState state{};
    };

    void applyStoredEditsToChunk(ChunkCoord coord);
    void markMeshNeighborhoodDirty(ChunkCoord coord, int localX, int localZ) noexcept;
    void markAdjacentChunksDirty(ChunkCoord coord) noexcept;
    void markAllLoadedDirty() noexcept;
    [[nodiscard]] bool microCollides(BlockCoord block, const micro::MicroVoxelState& state,
                                     float minX, float minY, float minZ,
                                     float maxX, float maxY, float maxZ) const noexcept;
    [[nodiscard]] const PromotedBlock* promotedBlock(BlockCoord position) const noexcept;

    std::uint32_t seed_{1337};
    float worldAgeSeconds_{};
    std::uint32_t growthEpoch_{};
    ChunkCoord streamCenter_{};
    ChunkManager chunks_;
    std::map<ChunkCoord, std::map<BlockCoord, BlockId>> editsByChunk_;
    std::map<ChunkCoord, std::map<BlockCoord, PromotedBlock>> microByChunk_;
    std::vector<ChunkCoord> recentlyUnloaded_;
};

} // namespace rf::world
