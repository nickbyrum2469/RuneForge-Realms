#include "world/streaming/ChunkStreamer.h"

#include "world/generation/TerrainGenerator.h"

#include <utility>

namespace rf::world::streaming {

ChunkStreamer::ChunkStreamer(std::size_t workerCount) : jobs_(workerCount) {}

ChunkStreamer::~ChunkStreamer() {
    jobs_.waitIdle();
}

void ChunkStreamer::reset() {
    jobs_.waitIdle();
    std::scoped_lock lock(mutex_);
    pending_.clear();
    completed_.clear();
}

bool ChunkStreamer::request(std::uint32_t seed, ChunkCoord coord) {
    {
        std::scoped_lock lock(mutex_);
        if (pending_.contains(coord)) return false;
        pending_.insert(coord);
    }

    jobs_.submit([this, seed, coord] {
        VoxelChunk chunk = generation::TerrainGenerator::generateChunk(seed, coord);
        std::scoped_lock lock(mutex_);
        completed_.push_back({coord, std::move(chunk)});
    });
    return true;
}

bool ChunkStreamer::pending(ChunkCoord coord) const {
    std::scoped_lock lock(mutex_);
    return pending_.contains(coord);
}

std::size_t ChunkStreamer::pendingCount() const {
    std::scoped_lock lock(mutex_);
    return pending_.size();
}

std::vector<CompletedChunk> ChunkStreamer::drainCompleted() {
    std::vector<CompletedChunk> result;
    std::scoped_lock lock(mutex_);
    result.reserve(completed_.size());
    while (!completed_.empty()) {
        pending_.erase(completed_.front().coord);
        result.push_back(std::move(completed_.front()));
        completed_.pop_front();
    }
    return result;
}

void ChunkStreamer::waitIdle() {
    jobs_.waitIdle();
}

} // namespace rf::world::streaming
