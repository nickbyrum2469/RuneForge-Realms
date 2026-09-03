#include "TestSuites.h"

#include "game/PlayerController.h"
#include "world/FrontierWorld.h"
#include "world/GreedyMesher.h"
#include "world/VoxelChunk.h"
#include "world/generation/TerrainGenerator.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

bool contains(const std::vector<rf::world::ChunkCoord>& coords, rf::world::ChunkCoord target) {
    return std::find(coords.begin(), coords.end(), target) != coords.end();
}

} // namespace

void runWorldTests() {
    rf::world::VoxelChunk single;
    single.set(1, 1, 1, rf::world::BlockId::Stone);
    const auto singleMesh = rf::world::GreedyMesher::build(single);
    assert(singleMesh.quadCount == 6);
    assert(singleMesh.vertices.size() == 24);
    assert(singleMesh.indices.size() == 36);

    rf::world::VoxelChunk solid;
    solid.fill(rf::world::BlockId::Stone);
    assert(rf::world::GreedyMesher::build(solid).quadCount == 6);

    rf::world::ChunkMeshingSnapshot joined;
    joined.center = solid;
    joined.positiveX = solid;
    assert(rf::world::GreedyMesher::build(joined).quadCount == 5);

    const auto generatedA = rf::world::generation::TerrainGenerator::generateChunk(424242u, {8, -3});
    const auto generatedB = rf::world::generation::TerrainGenerator::generateChunk(424242u, {8, -3});
    assert(generatedA.solidBlockCount() == generatedB.solidBlockCount());
    for (int y = 0; y < rf::world::VoxelChunk::sizeY; ++y) {
        assert(generatedA.get(2, y, 7) == generatedB.get(2, y, 7));
    }

    rf::world::FrontierWorld worldA;
    rf::world::FrontierWorld worldB;
    worldA.generate(424242u);
    worldB.generate(424242u);
    assert(worldA.loadedChunkCount() == 49);
    assert(worldA.solidBlockCount() == worldB.solidBlockCount());
    assert(worldA.getBlock(0, 0, 0) == worldB.getBlock(0, 0, 0));
    assert(worldA.topSolidY(0, 0) == worldB.topSolidY(0, 0));
    assert(worldA.chunkMeshingSnapshot({0, 0})->positiveX.has_value());
    assert(!worldA.buildMesh().empty());

    const int top = worldA.topSolidY(0, 0);
    const auto hit = worldA.raycast(0.5f, static_cast<float>(top) + 5.0f, 0.5f,
                                    0.0f, -1.0f, 0.0f, 8.0f);
    assert(hit.hit && hit.block.y == top);

    rf::game::PlayerController player;
    player.spawn({0.5f, static_cast<float>(top) + 4.0f, 0.5f});
    for (int i = 0; i < 180; ++i) player.update(1.0f / 60.0f, worldA);
    assert(player.grounded());
    assert(player.position().y >= static_cast<float>(top + 1) - 0.05f);

    // A modal/pause transition clears both held controls and one-shot input. In particular, a Space
    // keydown queued earlier in the same Win32 message pump must not turn into a surprise jump after
    // the menu closes and gameplay resumes.
    player.setControl(rf::game::MoveControl::Forward, true);
    player.setControl(rf::game::MoveControl::Sprint, true);
    player.setControl(rf::game::MoveControl::Crouch, true);
    player.requestJump();
    player.clearInputState();
    assert(!player.crouching());
    const auto beforeInputResetUpdate = player.position();
    player.update(1.0f / 60.0f, worldA);
    assert(std::abs(player.horizontalSpeed()) < 0.0001f);
    assert(player.position().y <= beforeInputResetUpdate.y + 0.001f);

    // Clear startup dirty state, then verify an edge edit invalidates both owning chunks.
    for (const auto coord : worldA.dirtyChunkCoords()) worldA.markChunkMeshQueued(coord);
    const int edgeTop = worldA.topSolidY(15, 1);
    assert(worldA.setBlock(15, edgeTop, 1, rf::world::BlockId::Air));
    const auto edgeDirty = worldA.dirtyChunkCoords();
    assert(contains(edgeDirty, {0, 0}));
    assert(contains(edgeDirty, {1, 0}));

    // Edits are independent of chunk residency and must survive eviction/regeneration.
    assert(worldA.setBlock(0, top, 0, rf::world::BlockId::Air));
    assert(worldA.getBlock(0, top, 0) == rf::world::BlockId::Air);
    assert(worldA.updateStreaming(16.0f * 20.0f + 0.5f, 0.5f));
    assert(worldA.streamingStats().pending <= rf::world::ChunkManager::maxPendingChunks);
    assert(worldA.updateStreaming(0.5f, 0.5f));
    assert(worldA.getBlock(0, top, 0) == rf::world::BlockId::Air);
}
