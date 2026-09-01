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
#include "world/generation/TerrainGenerator.h"

#include <atomic>
#include <cassert>
#include <filesystem>
#include <iostream>

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

    const auto& stoneDef = rf::world::blocks::BlockRegistry::get(rf::world::BlockId::Stone);
    assert(stoneDef.name == "Stone");
    assert(stoneDef.preferredTool == rf::world::blocks::ToolClass::Pickaxe);
    assert(stoneDef.hardness > 1.0f);
    const auto& stoneMaterial = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::Stone);
    assert(stoneMaterial.name == "weathered_stone");
    assert(stoneMaterial.detailProfile == "fractured_rock");

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

    // An edit must survive chunk eviction and deterministic regeneration.
    worldA.setBlock(0, top, 0, rf::world::BlockId::Air);
    assert(worldA.getBlock(0, top, 0) == rf::world::BlockId::Air);
    assert(worldA.updateStreaming(16.0f * 20.0f + 0.5f, 0.5f));
    assert(worldA.loadedChunkCount() >= 81);
    assert(worldA.updateStreaming(0.5f, 0.5f));
    assert(worldA.getBlock(0, top, 0) == rf::world::BlockId::Air);

    // The generic worker pool is deliberately independent from world state for now; the next
    // pass can hand chunk-generation and meshing jobs to it without coupling threading to gameplay.
    std::atomic<int> completed{0};
    {
        rf::core::jobs::JobSystem jobs(2);
        for (int i = 0; i < 64; ++i) jobs.submit([&completed] { completed.fetch_add(1); });
        jobs.waitIdle();
        assert(jobs.pendingJobs() == 0);
    }
    assert(completed.load() == 64);

    const auto temp = std::filesystem::temp_directory_path() / "runeforge-frontier-core-test.rfsv";
    rf::save::FrontierSaveData saveData;
    saveData.seed = 424242u;
    saveData.playerPosition = player.position();
    saveData.yaw = 0.75f;
    saveData.pitch = -0.2f;
    saveData.edits = worldA.edits();
    assert(rf::save::saveFrontierSave(temp, saveData));
    const auto loaded = rf::save::loadFrontierSave(temp);
    assert(loaded);
    assert(loaded->seed == saveData.seed);
    assert(loaded->edits.size() == saveData.edits.size());
    std::error_code ec;
    std::filesystem::remove(temp, ec);

    std::cout << "RuneForge Frontier streaming/core tests passed\n";
    return 0;
}
