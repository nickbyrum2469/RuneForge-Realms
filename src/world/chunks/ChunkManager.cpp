#include "world/chunks/ChunkManager.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace rf::world {

void ChunkManager::clear() noexcept {
    chunks_.clear();
}

ChunkStreamDelta ChunkManager::update(ChunkCoord center,
                                      int loadRadius,
                                      int retainRadius,
                                      const Generator& generator) {
    loadRadius = std::max(loadRadius, 0);
    retainRadius = std::max(retainRadius, loadRadius);

    ChunkStreamDelta delta;
    std::vector<ChunkCoord> requested;
    requested.reserve(static_cast<std::size_t>((loadRadius * 2 + 1) * (loadRadius * 2 + 1)));

    for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
        for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
            const ChunkCoord coord{center.x + dx, center.z + dz};
            if (!contains(coord)) requested.push_back(coord);
        }
    }

    std::sort(requested.begin(), requested.end(), [center](ChunkCoord a, ChunkCoord b) {
        const int ad = chebyshevDistance(a, center);
        const int bd = chebyshevDistance(b, center);
        if (ad != bd) return ad < bd;
        const int am = std::abs(a.x - center.x) + std::abs(a.z - center.z);
        const int bm = std::abs(b.x - center.x) + std::abs(b.z - center.z);
        if (am != bm) return am < bm;
        return a < b;
    });

    for (const ChunkCoord coord : requested) {
        insert(coord, generator(coord), ChunkState::Dirty);
        delta.loaded.push_back(coord);
    }

    delta.unloaded = evictOutside(center, retainRadius);
    return delta;
}

void ChunkManager::insert(ChunkCoord coord, VoxelChunk chunk, ChunkState state) {
    chunks_.insert_or_assign(coord, Record{std::move(chunk), state});
}

std::vector<ChunkCoord> ChunkManager::evictOutside(ChunkCoord center, int retainRadius) {
    retainRadius = std::max(retainRadius, 0);
    std::vector<ChunkCoord> removed;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        if (chebyshevDistance(it->first, center) > retainRadius) {
            removed.push_back(it->first);
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
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

void ChunkManager::markDirty(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    if (it != chunks_.end()) it->second.state = ChunkState::Dirty;
}

void ChunkManager::markReady(ChunkCoord coord) noexcept {
    const auto it = chunks_.find(coord);
    if (it != chunks_.end()) it->second.state = ChunkState::Ready;
}

} // namespace rf::world
