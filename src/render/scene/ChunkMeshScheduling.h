#pragma once

#include "world/chunks/ChunkCoord.h"

namespace rf::render::scene {

// A chunk may have at most one asynchronous mesh build in flight. If its world revision or
// distance-detail tier changes while that job is running, the completed stale result is discarded
// by the renderer and the newest state is queued on the following scheduling pass. Coalescing by
// chunk coordinate prevents rapid edits/LOD changes from filling the bounded worker queue with many
// obsolete builds for the same chunk and starving untouched nearby chunks.
template <typename PendingRange>
[[nodiscard]] bool chunkMeshCoordPending(const PendingRange& pendingMeshes,
                                         world::ChunkCoord coord) noexcept {
    for (const auto& pending : pendingMeshes) {
        if (pending.coord == coord) return true;
    }
    return false;
}

} // namespace rf::render::scene
