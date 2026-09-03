#pragma once

#include "core/jobs/JobSystem.h"
#include "world/VoxelChunk.h"
#include "world/chunks/ChunkState.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <set>
#include <vector>

namespace rf::world {

struct ChunkStreamingStats {
    std::size_t loaded{};
    std::size_t pending{};
    ChunkCoord center{};
};

class ChunkManager {
public:
    using Generator = std::function<VoxelChunk(ChunkCoord)>;

    static constexpr int prefetchBudgetPerUpdate = 10;
    static constexpr std::size_t maxPendingChunks = 128;

    struct Record {
        VoxelChunk voxels;
        ChunkState state{ChunkState::Ready};
        std::uint64_t revision{1};
    };

    ChunkManager();

    void clear() noexcept;

    [[nodiscard]] ChunkStreamDelta update(ChunkCoord center,
                                          int residentRadius,
                                          int prefetchRadius,
                                          int retainRadius,
                                          const Generator& generator);

    [[nodiscard]] VoxelChunk* find(ChunkCoord coord) noexcept;
    [[nodiscard]] const VoxelChunk* find(ChunkCoord coord) const noexcept;
    [[nodiscard]] bool contains(ChunkCoord coord) const noexcept;
    [[nodiscard]] std::size_t loadedCount() const noexcept { return chunks_.size(); }
    [[nodiscard]] std::size_t pendingCount() const noexcept { return pending_.size(); }
    [[nodiscard]] std::vector<ChunkCoord> loadedCoords() const;
    [[nodiscard]] std::vector<ChunkCoord> dirtyCoords() const;
    [[nodiscard]] std::uint64_t revision(ChunkCoord coord) const noexcept;
    [[nodiscard]] ChunkStreamingStats stats() const noexcept;

    void markDirty(ChunkCoord coord) noexcept;
    void markReady(ChunkCoord coord) noexcept;

private:
    struct PendingChunk {
        ChunkCoord coord{};
        std::future<VoxelChunk> future;
    };

    bool ensureResident(ChunkCoord coord, const Generator& generator, ChunkStreamDelta& delta);
    bool consumePending(ChunkCoord coord, ChunkStreamDelta& delta);
    void pumpCompleted(ChunkCoord center, int retainRadius, ChunkStreamDelta& delta);
    void schedulePrefetch(ChunkCoord center, int residentRadius, int prefetchRadius, const Generator& generator);
    void unloadFar(ChunkCoord center, int retainRadius, ChunkStreamDelta& delta);

    std::map<ChunkCoord, Record> chunks_;
    std::vector<PendingChunk> pending_;
    std::set<ChunkCoord> queued_;
    core::jobs::JobSystem jobs_;
    ChunkCoord center_{};
};

} // namespace rf::world
