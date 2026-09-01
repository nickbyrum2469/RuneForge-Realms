#include "core/HubModel.h"
#include "core/Version.h"
#include "core/jobs/JobSystem.h"
#include "game/PlayerController.h"
#include "render/materials/MaterialRegistry.h"
#include "save/FrontierSave.h"
#include "world/FrontierWorld.h"
#include "world/GreedyMesher.h"
#include "world/VoxelChunk.h"
#include "world/blocks/BlockRegistry.h"
#include "world/chunks/ChunkManager.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <vector>

int main() {
    const auto a = rf::Version::parse("0.1.0");
    const auto b = rf::Version::parse("v0.3.1");
    assert(a && b && *b > *a);
    assert(a->toString() == "0.1.0");
    assert(!rf::Version::parse("0.1"));

    rf::HubModel hub;
    assert(!hub.hasSave());
    hub.setHasSave(true);
    assert(hub.hasSave());
    hub.selectNav(99);
    assert(hub.selectedNavIndex() == 3);

    // The worker pool is an engine primitive, not tied to rendering or world code.
    rf::core::jobs::JobSystem jobs(2);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 16; ++i) futures.push_back(jobs.submit([i]() { return i * i; }));
    for (int i = 0; i < 16; ++i) assert(futures[static_cast<std::size_t>(i)].get() == i * i);
    jobs.waitIdle();

    const auto& stoneDefinition = rf::world::blocks::BlockRegistry::get(rf::world::BlockId::Stone);
    assert(stoneDefinition.key == "stone");
    assert(stoneDefinition.hardness > rf::world::blocks::BlockRegistry::get(rf::world::BlockId::Dirt).hardness);
    const auto& grassMaterial = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::Grass);
    assert(grassMaterial.key == "grass");
    assert(grassMaterial.roughness > 0.5f);

    rf::world::VoxelChunk single;
    single.set(1, 1, 1, rf::world::BlockId::Stone);
    const auto singleMesh = rf::world::GreedyMesher::build(single);
    assert(singleMesh.quadCount == 6);
    assert(singleMesh.vertices.size() == 24);
    assert(singleMesh.indices.size() == 36);

    rf::world::VoxelChunk solid;
    solid.fill(rf::world::BlockId::Stone);
    const auto solidMesh = rf::world::GreedyMesher::build(solid);
    assert(solidMesh.quadCount == 6);

    // Chunk streaming always keeps a synchronous safety ring and asynchronously prefetches farther terrain.
    rf::world::ChunkManager manager;
    manager.reset(424242u);
    const auto firstStream = manager.update(0, 0);
    assert(firstStream.loaded.size() >= 25);
    assert(manager.stats().loaded >= 25);
    const auto farStream = manager.update(rf::world::VoxelChunk::sizeX * 9, 0);
    assert(manager.isLoaded({9, 0}));
    assert(!farStream.unloaded.empty());

    rf::world::FrontierWorld worldA;
    rf::world::FrontierWorld worldB;
    worldA.generate(424242u);
    worldB.generate(424242u);
    assert(worldA.solidBlockCount() == worldB.solidBlockCount());
    assert(worldA.getBlock(0, 0, 0) == worldB.getBlock(0, 0, 0));
    assert(worldA.topSolidY(0, 0) == worldB.topSolidY(0, 0));
    assert(worldA.streamingStats().loaded >= 25);
    const auto worldMesh = worldA.buildMesh();
    assert(!worldMesh.empty());
    assert(worldMesh.quadCount > 6);

    const int top = worldA.topSolidY(0, 0);
    const auto hit = worldA.raycast(0.5f, static_cast<float>(top) + 5.0f, 0.5f, 0.0f, -1.0f, 0.0f, 8.0f);
    assert(hit.hit);
    assert(hit.block.y == top);

    rf::game::PlayerController player;
    player.spawn({0.5f, static_cast<float>(top) + 4.0f, 0.5f});
    for (int i = 0; i < 180; ++i) player.update(1.0f / 60.0f, worldA);
    assert(player.grounded());
    assert(player.position().y >= static_cast<float>(top + 1) - 0.05f);

    // Schema 2 stores edits in region files while keeping metadata small. Use multiple regions to test partitioning.
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto tempRoot = std::filesystem::temp_directory_path() / ("runeforge-frontier-core-test-" + std::to_string(nonce));
    const auto tempSave = tempRoot / "world.rfsv";
    rf::save::FrontierSaveData saveData;
    saveData.seed = 424242u;
    saveData.playerPosition = player.position();
    saveData.yaw = 0.75f;
    saveData.pitch = -0.2f;
    saveData.edits.push_back({{1, top, 1}, rf::world::BlockId::Air});
    saveData.edits.push_back({{600, 4, -600}, rf::world::BlockId::Stone});
    assert(rf::save::saveFrontierSave(tempSave, saveData));
    assert(std::filesystem::is_directory(tempRoot / "regions"));
    const auto loaded = rf::save::loadFrontierSave(tempSave);
    assert(loaded);
    assert(loaded->seed == saveData.seed);
    assert(loaded->edits.size() == saveData.edits.size());

    // A full save with no edits must remove stale region files rather than resurrecting an old world's edits.
    saveData.edits.clear();
    assert(rf::save::saveFrontierSave(tempSave, saveData));
    const auto cleared = rf::save::loadFrontierSave(tempSave);
    assert(cleared && cleared->edits.empty());

    // Existing 0.3.0 schema-1 saves remain readable and are migrated on the next save.
    const auto legacySave = tempRoot / "legacy.rfsv";
    {
        std::ofstream output(legacySave);
        output << "RUNEFORGE_FRONTIER_SAVE 1\n";
        output << "seed 999\n";
        output << "player 1 2 3 0.1 -0.2\n";
        output << "edits 1\n";
        output << "4 5 6 " << static_cast<int>(rf::world::BlockId::Dirt) << "\n";
    }
    const auto legacy = rf::save::loadFrontierSave(legacySave);
    assert(legacy && legacy->seed == 999u && legacy->edits.size() == 1);

    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);

    std::cout << "RuneForge engine-scale core tests passed\n";
    return 0;
}
