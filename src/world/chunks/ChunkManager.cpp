#include "world/chunks/ChunkManager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace rf::world {

ChunkManager::ChunkManager() = default;

void ChunkManager::clear() noexcept {
    jobs_.waitIdle();
    chunks_.clear();
    pending_.clear();
    queued_.clear();
    center_ = {};
}

bool ChunkManager::consumePending(ChunkCoord coord, ChunkStreamDelta& delta) {
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->coord != coord) continue;
        // A prefetched chunk may enter the resident window before its worker finishes. Do not turn
        // normal traversal into a main-thread future::get stall; pumpCompleted installs it later.
        if (it->future.wait_for(std::chrono::seconds{0}) != std::future_status::ready) return false;
        VoxelChunk chunk = it->future.get();
        queued_.erase(coord);
        pending_.erase(it);
        chunks_.insert_or_assign(coord, Record{std::move(chunk), ChunkState::Dirty, 1});
        delta.loaded.push_back(coord);
        return true;
    }
    return false;
}

bool ChunkManager::ensureResident(ChunkCoord coord, const Generator& generator, ChunkStreamDelta& delta) {
    if (contains(coord)) return false;
    if (queued_.contains(coord)) return consumePending(coord, delta);
    chunks_.emplace(coord, Record{generator(coord), ChunkState::Dirty, 1});
    delta.loaded.push_back(coord);
    return true;
}

void ChunkManager::pumpCompleted(ChunkStreamDelta& delta) {
    using namespace std::chrono_literals;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->future.wait_for(0ms) != std::future_status::ready) {
            ++it;
            continue;
        }
        const ChunkCoord coord = it->coord;
        VoxelChunk chunk = it->future.get();
        queued_.erase(coord);
        it = pending_.erase(it);
        if (chunks_.contains(coord)) continue;
        chunks_.emplace(coord, Record{std::move(chunk), ChunkState::Dirty, 1});
        delta.loaded.push_back(coord);
    }
}

void ChunkManager::schedulePrefetch(ChunkCoord center,
                                    int residentRadius,
                                    int prefetchRadius,
                                    const Generator& generator) {
    if (pending_.size() >= maxPendingChunks) return;

    int scheduled = 0;
    for (int radius = residentRadius + 1;
         radius <= prefetchRadius && scheduled < prefetchBudgetPerUpdate && pending_.size() < maxPendingChunks;
         ++radius) {
        for (int dz = -radius;
             dz <= radius && scheduled < prefetchBudgetPerUpdate && pending_.size() < maxPendingChunks;
             ++dz) {
            for (int dx = -radius;
                 dx <= radius && scheduled < prefetchBudgetPerUpdate && pending_.size() < maxPendingChunks;
                 ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const ChunkCoord coord{center.x + dx, center.z + dz};
                if (chunks_.contains(coord) || queued_.contains(coord)) continue;
                const Generator safeGenerator = generator;
                pending_.push_back(PendingChunk{
                    coord,
                    jobs_.submitResult([safeGenerator, coord]() { return safeGenerator(coord); }),
                });
                queued_.insert(coord);
                ++scheduled;
            }
        }
    }
}

void ChunkManager::unloadFar(ChunkCoord center, int retainRadius, ChunkStreamDelta& delta) {
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        if (chebyshevDistance(it->first, center) <= retainRadius) {
            ++it;
            continue;
        }
        delta.unloaded.push_back(it->first);
        it = chunks_.erase(it);
    }
}

ChunkStreamDelta ChunkManager::update(ChunkCoord center,
                                      int residentRadius,
                                      int prefetchRadius,
                                      int retainRadius,
                                      const Generator& generator) {
    residentRadius = std::max(residentRadius, 0);
    prefetchRadius = std::max(prefetchRadius, residentRadius);
    retainRadius = std::max(retainRadius, prefetchRadius);
    center_ = center;

    ChunkStreamDelta delta;
    pumpCompleted(delta);

    for (int dz = -residentRadius; dz <= residentRadius; ++dz) {
        for (int dx = -residentRadius; dx <= residentRadius; ++dx) {
            ensureResident({center.x + dx, center.z + dz}, generator, delta);
        }
    }

    schedulePrefetch(center, residentRadius, prefetchRadius, generator);
    unloadFar(center, retainRadius, delta);
    return delta;
}

VoxelChunk* ChunkManager::find(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    return it == chunks_.end() ? nullptr : &it->second.voxels;
}

const VoxelChunk* ChunkManager::find(ChunkCoord coord) const noexcept {
    const auto it = chunks_.find(coord);
    return it == chunks_.end() ? nullptr : &it->second.voxels;
}

bool ChunkManager::contains(ChunkCoord coord) const noexcept {
    return chunks_.contains(coord);
}

std::vector<ChunkCoord> ChunkManager::loadedCoords() const {
    std::vector<ChunkCoord> result;
    result.reserve(chunks_.size());
    for (const auto& [coord, record] : chunks_) {
        (void)record;
        result.push_back(coord);
    }
    return result;
}

std::vector<ChunkCoord> ChunkManager::dirtyCoords() const {
    std::vector<ChunkCoord> result;
    for (const auto& [coord, record] : chunks_) {
        if (record.state == ChunkState::Dirty) result.push_back(coord);
    }
    return result;
}

std::uint64_t ChunkManager::revision(ChunkCoord coord) const noexcept {
    const auto it = chunks_.find(coord);
    return it == chunks_.end() ? 0 : it->second.revision;
}

ChunkStreamingStats ChunkManager::stats() const noexcept {
    return {chunks_.size(), pending_.size(), center_};
}

void ChunkManager::markDirty(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    if (it == chunks_.end()) return;
    it->second.state = ChunkState::Dirty;
    ++it->second.revision;
}

void ChunkManager::markReady(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    if (it != chunks_.end()) it->second.state = ChunkState::Ready;
}

} // namespace rf::world
