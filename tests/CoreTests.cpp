#include "core/HubModel.h"
#include "core/Version.h"
#include "game/PlayerController.h"
#include "save/FrontierSave.h"
#include "world/FrontierWorld.h"
#include "world/GreedyMesher.h"
#include "world/VoxelChunk.h"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const auto a = rf::Version::parse("0.1.0");
    const auto b = rf::Version::parse("v0.3.0");
    assert(a && b && *b > *a);
    assert(a->toString() == "0.1.0");
    assert(!rf::Version::parse("0.1"));

    rf::HubModel hub;
    assert(!hub.hasSave());
    hub.setHasSave(true);
    assert(hub.hasSave());
    hub.selectNav(99);
    assert(hub.selectedNavIndex() == 3);

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

    rf::world::FrontierWorld worldA;
    rf::world::FrontierWorld worldB;
    worldA.generate(424242u);
    worldB.generate(424242u);
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

    const auto temp = std::filesystem::temp_directory_path() / "runeforge-frontier-core-test.rfsv";
    rf::save::FrontierSaveData saveData;
    saveData.seed = 424242u;
    saveData.playerPosition = player.position();
    saveData.yaw = 0.75f;
    saveData.pitch = -0.2f;
    worldA.setBlock(1, top, 1, rf::world::BlockId::Air);
    saveData.edits = worldA.edits();
    assert(rf::save::saveFrontierSave(temp, saveData));
    const auto loaded = rf::save::loadFrontierSave(temp);
    assert(loaded);
    assert(loaded->seed == saveData.seed);
    assert(loaded->edits.size() == saveData.edits.size());
    std::error_code ec;
    std::filesystem::remove(temp, ec);

    std::cout << "RuneForge Frontier core tests passed\n";
    return 0;
}
