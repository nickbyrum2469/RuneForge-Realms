#pragma once

#include "core/jobs/JobSystem.h"
#include "world/VoxelChunk.h"
#include "world/chunks/ChunkCoord.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <vector>

namespace rf::world::streaming {

struct CompletedChunk {
    ChunkCoord coord{};
    VoxelChunk voxels{};
};

class ChunkStreamer {
public:
    explicit ChunkStreamer(std::size_t workerCount = 0);
    ~ChunkStreamer();

    ChunkStreamer(const ChunkStreamer&) = delete;
    ChunkStreamer& operator=(const ChunkStreamer&) = delete;

    void reset();
    [[nodiscard]] bool request(std::uint32_t seed, ChunkCoord coord);
    [[nodiscard]] bool pending(ChunkCoord coord) const;
    [[nodiscard]] std::size_t pendingCount() const;
    [[nodiscard]] std::size_t workerCount() const noexcept { return jobs_.workerCount(); }
    [[nodiscard]] std::vector<CompletedChunk> drainCompleted();
    void waitIdle();

private:
    core::jobs::JobSystem jobs_;
    mutable std::mutex mutex_;
    std::set<ChunkCoord> pending_;
    std::deque<CompletedChunk> completed_;
};

} // namespace rf::world::streaming
