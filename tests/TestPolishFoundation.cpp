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

    // Water is a macro-geography feature. Search a deterministic 9x9 chunk window instead of
    // requiring the origin chunk itself to be wet for every seed.
    bool sawWater = false;
    bool sawDryLand = false;
    for (int cz = -4; cz <= 4; ++cz) {
        for (int cx = -4; cx <= 4; ++cx) {
            const auto chunk = world::generation::TerrainGenerator::generateChunk(1337u, {cx, cz});
            for (int z = 0; z < world::VoxelChunk::sizeZ; ++z) {
                for (int x = 0; x < world::VoxelChunk::sizeX; ++x) {
                    for (int y = 0; y < world::VoxelChunk::sizeY; ++y) {
                        const auto block = chunk.get(x, y, z);
                        sawWater = sawWater || block == world::BlockId::Water;
                        sawDryLand = sawDryLand || block == world::BlockId::Grass;
                    }
                }
            }
        }
    }
    assert(sawWater);
    assert(sawDryLand);
}
