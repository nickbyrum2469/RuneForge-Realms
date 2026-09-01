#include "world/chunks/ChunkManager.h"

#include "world/generation/TerrainGenerator.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace rf::world {

ChunkManager::ChunkManager() = default;

ChunkCoord ChunkManager::worldToChunk(int worldBlockX, int worldBlockZ) noexcept {
    return {floorDiv(worldBlockX, VoxelChunk::sizeX), floorDiv(worldBlockZ, VoxelChunk::sizeZ)};
}

int ChunkManager::chebyshevDistance(ChunkCoord a, ChunkCoord b) noexcept {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

void ChunkManager::reset(std::uint32_t seed) {
    seed_ = seed;
    center_ = {};
    chunks_.clear();
    pending_.clear();
    queued_.clear();
}

bool ChunkManager::consumePendingSynchronously(ChunkCoord coord, std::vector<ChunkCoord>* newlyLoaded) {
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->coord != coord) continue;
        VoxelChunk chunk = it->future.get();
        queued_.erase(coord);
        pending_.erase(it);
        chunks_.insert_or_assign(coord, ChunkRecord{std::move(chunk), 1, true});
        if (newlyLoaded) newlyLoaded->push_back(coord);
        return true;
    }
    return false;
}

bool ChunkManager::ensureLoaded(ChunkCoord coord, std::vector<ChunkCoord>* newlyLoaded) {
    if (chunks_.contains(coord)) return false;
    if (queued_.contains(coord)) return consumePendingSynchronously(coord, newlyLoaded);
    auto chunk = generation::TerrainGenerator::generateChunk(seed_, coord);
    chunks_.emplace(coord, ChunkRecord{std::move(chunk), 1, true});
    if (newlyLoaded) newlyLoaded->push_back(coord);
    return true;
}

void ChunkManager::pumpFinished(ChunkStreamUpdate& update) {
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
        if (chebyshevDistance(coord, center_) > unloadRadius) continue;
        if (chunks_.contains(coord)) continue;
        chunks_.emplace(coord, ChunkRecord{std::move(chunk), 1, true});
        update.loaded.push_back(coord);
    }
}

void ChunkManager::schedulePrefetch() {
    int scheduled = 0;
    for (int radius = residentRadius + 1; radius <= prefetchRadius && scheduled < scheduleBudgetPerUpdate; ++radius) {
        for (int dz = -radius; dz <= radius && scheduled < scheduleBudgetPerUpdate; ++dz) {
            for (int dx = -radius; dx <= radius && scheduled < scheduleBudgetPerUpdate; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const ChunkCoord coord{center_.x + dx, center_.z + dz};
                if (chunks_.contains(coord) || queued_.contains(coord)) continue;
                const std::uint32_t seed = seed_;
                pending_.push_back(PendingChunk{
                    coord,
                    jobs_.submit([seed, coord]() { return generation::TerrainGenerator::generateChunk(seed, coord); }),
                });
                queued_.insert(coord);
                ++scheduled;
            }
        }
    }
}

void ChunkManager::unloadFar(ChunkStreamUpdate& update) {
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        if (chebyshevDistance(it->first, center_) <= unloadRadius) {
            ++it;
            continue;
        }
        update.unloaded.push_back(it->first);
        it = chunks_.erase(it);
    }
}

ChunkStreamUpdate ChunkManager::update(int worldBlockX, int worldBlockZ) {
    center_ = worldToChunk(worldBlockX, worldBlockZ);
    ChunkStreamUpdate result;
    pumpFinished(result);

    for (int dz = -residentRadius; dz <= residentRadius; ++dz) {
        for (int dx = -residentRadius; dx <= residentRadius; ++dx) {
            ensureLoaded({center_.x + dx, center_.z + dz}, &result.loaded);
        }
    }

    schedulePrefetch();
    unloadFar(result);
    return result;
}

VoxelChunk* ChunkManager::chunkAt(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    return it == chunks_.end() ? nullptr : &it->second.chunk;
}

const VoxelChunk* ChunkManager::chunkAt(ChunkCoord coord) const noexcept {
    const auto it = chunks_.find(coord);
    return it == chunks_.end() ? nullptr : &it->second.chunk;
}

bool ChunkManager::isLoaded(ChunkCoord coord) const noexcept {
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

void ChunkManager::markDirty(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    if (it == chunks_.end()) return;
    ++it->second.revision;
    it->second.meshDirty = true;
}

std::vector<ChunkCoord> ChunkManager::takeDirtyChunks() {
    std::vector<ChunkCoord> result;
    for (auto& [coord, record] : chunks_) {
        if (!record.meshDirty) continue;
        record.meshDirty = false;
        result.push_back(coord);
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

} // namespace rf::world
