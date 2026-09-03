#include "render/scene/ChunkMeshScheduling.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {
struct PendingMeshStub {
    rf::world::ChunkCoord coord{};
    std::uint64_t revision{};
    int detailTier{};
};
}

void runMeshSchedulingTests() {
    using rf::render::scene::chunkMeshCoordPending;
    using rf::world::ChunkCoord;

    std::vector<PendingMeshStub> pending{
        {{1, -2}, 7, 2},
        {{0, 0}, 3, 1},
    };

    assert(chunkMeshCoordPending(pending, ChunkCoord{1, -2}));
    assert(chunkMeshCoordPending(pending, ChunkCoord{0, 0}));
    assert(!chunkMeshCoordPending(pending, ChunkCoord{2, -2}));

    // A newer world revision or different detail tier for the same coordinate must still be
    // coalesced behind the in-flight job. Once that job completes, renderer revision/tier checks
    // discard stale output and the newest state can be queued next.
    pending.push_back({{1, -2}, 99, 0});
    assert(chunkMeshCoordPending(pending, ChunkCoord{1, -2}));

    pending.clear();
    assert(!chunkMeshCoordPending(pending, ChunkCoord{1, -2}));
}
