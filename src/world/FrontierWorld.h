#pragma once

#include "world/GreedyMesher.h"
#include "world/chunks/ChunkManager.h"
#include "world/streaming/ChunkStreamer.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace rf::world {

struct BlockCoord {
    int x{};
    int y{};
    int z{};
    auto operator<=>(const BlockCoord&) const = default;
};

struct BlockEdit {
    BlockCoord position{};
    BlockId block{BlockId::Air};
};

struct RaycastHit {
    bool hit{false};
    BlockCoord block{};
    BlockCoord adjacent{};
};

class FrontierWorld {
public:
    static constexpr int initialChunkRadius = 3;
    static constexpr int streamingLoadRadius = 4;
    static constexpr int streamingRetainRadius = 5;

    void generate(std::uint32_t seed, ChunkCoord initialCenter = {});
    [[nodiscard]] bool updateStreaming(float worldX, float worldZ);
    void waitForStreamingIdle();

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
    [[nodiscard]] std::size_t solidBlockCount() const noexcept;
    [[nodiscard]] std::size_t loadedChunkCount() const noexcept { return chunks_.loadedCount(); }
    [[nodiscard]] std::size_t pendingChunkCount() const { return streamer_.pendingCount(); }
    [[nodiscard]] std::size_t streamingWorkerCount() const noexcept { return streamer_.workerCount(); }
    [[nodiscard]] std::vector<ChunkCoord> loadedChunkCoords() const { return chunks_.loadedCoords(); }
    [[nodiscard]] std::vector<ChunkCoord> dirtyChunkCoords() const { return chunks_.dirtyCoords(); }
    void markChunkMeshReady(ChunkCoord coord) { chunks_.markReady(coord); }
    void markChunkMeshesReady();

    [[nodiscard]] std::vector<BlockEdit> edits() const;
    void applyEdit(const BlockEdit& edit);
    void clearEdits() noexcept { edits_.clear(); }

private:
    [[nodiscard]] VoxelChunk generateChunk(ChunkCoord coord) const;
    void applyStoredEditsToChunk(ChunkCoord coord, VoxelChunk& chunk) const;
    void requestMissingChunks(ChunkCoord center);

    std::uint32_t seed_{1337};
    ChunkCoord streamCenter_{};
    ChunkManager chunks_;
    streaming::ChunkStreamer streamer_;
    std::map<BlockCoord, BlockId> edits_;
};

} // namespace rf::world
