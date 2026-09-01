#include "TestSuites.h"

#include "core/settings/GameSettings.h"
#include "game/mining/MiningCadence.h"
#include "game/mining/MiningSystem.h"
#include "world/Block.h"
#include "world/blocks/BlockRegistry.h"
#include "world/generation/TerrainGenerator.h"

#include <cassert>

void runPolishFoundationTests() {
    using namespace rf;

    game::mining::MiningCadence cadence;
    cadence.press();
    assert(cadence.update(0.0f, 0.50f));
    cadence.release();
    cadence.press();
    assert(!cadence.update(0.10f, 0.50f)); // click spam cannot bypass the existing cooldown.
    cadence.release();
    assert(!cadence.update(0.39f, 0.50f));
    cadence.press();
    assert(cadence.update(0.02f, 0.50f));

    const float dirtHand = game::mining::MiningSystem::strikeInterval(world::BlockId::Dirt);
    const float stoneHand = game::mining::MiningSystem::strikeInterval(world::BlockId::Stone);
    assert(stoneHand > dirtHand);
    assert(world::blocks::BlockRegistry::get(world::BlockId::Stone).minimumToolTier == 1);

    core::settings::GameSettings settings;
    settings.mouseSensitivity = 99.0f;
    settings.fovDegrees = 10.0f;
    settings.foliageQuality = 8;
    settings.sanitize();
    assert(settings.mouseSensitivity == 2.50f);
    assert(settings.fovDegrees == 65.0f);
    assert(settings.foliageQuality == 2);

    assert(world::isRenderable(world::BlockId::Water));
    assert(!world::isCollidable(world::BlockId::Water));
    assert(!world::isOpaque(world::BlockId::Water));

    const auto wetChunk = world::generation::TerrainGenerator::generateChunk(1337u, {0, 0});
    bool sawWater = false;
    for (int z = 0; z < world::VoxelChunk::sizeZ; ++z) {
        for (int x = 0; x < world::VoxelChunk::sizeX; ++x) {
            for (int y = 0; y < world::VoxelChunk::sizeY; ++y) {
                if (wetChunk.get(x, y, z) == world::BlockId::Water) sawWater = true;
            }
        }
    }
    // The macro generator should produce water in ordinary terrain seeds rather than making it mythical.
    assert(sawWater);
}
