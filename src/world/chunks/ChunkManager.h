#pragma once

#include "core/jobs/JobSystem.h"
#include "world/VoxelChunk.h"
#include "world/chunks/ChunkCoord.h"

#include <cstddef>
#include <cstdint>
#include <future>
#include <map>
#include <set>
#include <vector>

namespace rf::world {

struct ChunkStreamUpdate {
    std::vector<ChunkCoord> loaded;
    std::vector<ChunkCoord> unloaded;

    [[nodiscard]] bool geometryChanged() const noexcept { return !loaded.empty() || !unloaded.empty(); }
};

struct ChunkStreamingStats {
    std::size_t loaded{};
    std::size_t pending{};
    ChunkCoord center{};
};

class ChunkManager {
public:
    static constexpr int residentRadius = 2;
    static constexpr int prefetchRadius = 5;
    static constexpr int unloadRadius = 7;
    static constexpr int scheduleBudgetPerUpdate = 10;

    ChunkManager();

    void reset(std::uint32_t seed);
    [[nodiscard]] ChunkStreamUpdate update(int worldBlockX, int worldBlockZ);
    bool ensureLoaded(ChunkCoord coord, std::vector<ChunkCoord>* newlyLoaded = nullptr);

    [[nodiscard]] VoxelChunk* chunkAt(ChunkCoord coord) noexcept;
    [[nodiscard]] const VoxelChunk* chunkAt(ChunkCoord coord) const noexcept;
    [[nodiscard]] bool isLoaded(ChunkCoord coord) const noexcept;
    [[nodiscard]] std::vector<ChunkCoord> loadedCoords() const;

    void markDirty(ChunkCoord coord) noexcept;
    [[nodiscard]] std::vector<ChunkCoord> takeDirtyChunks();
    [[nodiscard]] std::uint64_t revision(ChunkCoord coord) const noexcept;
    [[nodiscard]] ChunkStreamingStats stats() const noexcept;
    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }

private:
    struct ChunkRecord {
        VoxelChunk chunk;
        std::uint64_t revision{1};
        bool meshDirty{true};
    };

    struct PendingChunk {
        ChunkCoord coord{};
        std::future<VoxelChunk> future;
    };

    [[nodiscard]] static ChunkCoord worldToChunk(int worldBlockX, int worldBlockZ) noexcept;
    [[nodiscard]] static int chebyshevDistance(ChunkCoord a, ChunkCoord b) noexcept;
    void pumpFinished(ChunkStreamUpdate& update);
    void schedulePrefetch();
    void unloadFar(ChunkStreamUpdate& update);
    bool consumePendingSynchronously(ChunkCoord coord, std::vector<ChunkCoord>* newlyLoaded);

    std::uint32_t seed_{1337};
    ChunkCoord center_{};
    std::map<ChunkCoord, ChunkRecord> chunks_;
    std::vector<PendingChunk> pending_;
    std::set<ChunkCoord> queued_;
    core::jobs::JobSystem jobs_;
};

} // namespace rf::world
