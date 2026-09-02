#include "TestSuites.h"

#include "core/settings/GameSettings.h"
#include "game/interaction/MiningSwing.h"
#include "game/mining/MiningCadence.h"
#include "game/mining/MiningSystem.h"
#include "game/particles/ParticleSystem.h"
#include "world/Block.h"
#include "world/FrontierWorld.h"
#include "world/blocks/BlockRegistry.h"
#include "world/generation/TerrainGenerator.h"

#include <cassert>
#include <optional>

void runPolishFoundationTests() {
    using namespace rf;

    game::mining::MiningCadence cadence;
    cadence.press();
    assert(cadence.update(0.0f, 0.50f));
    cadence.release();
    cadence.press();
    assert(!cadence.update(0.10f, 0.50f)); // click spam cannot bypass the existing cooldown.
    cadence.release();
    assert(!cadence.update(0.25f, 0.50f));
    cadence.press();
    assert(cadence.update(0.16f, 0.50f));

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

    // Breaking support beneath a lake activates only the local fluid neighborhood. The water then
    // falls into the opening on scheduled simulation ticks instead of scanning every lake cell/frame.
    world::FrontierWorld flowWorld;
    flowWorld.generate(1337u);
    std::optional<world::BlockCoord> supportedWater;
    for (int z = -48; z <= 48 && !supportedWater; ++z) {
        for (int x = -48; x <= 48 && !supportedWater; ++x) {
            for (int y = 2; y < world::VoxelChunk::sizeY && !supportedWater; ++y) {
                if (flowWorld.getBlock(x, y, z) != world::BlockId::Water) continue;
                if (!world::isSolid(flowWorld.getBlock(x, y - 1, z))) continue;
                supportedWater = world::BlockCoord{x, y, z};
            }
        }
    }
    assert(supportedWater.has_value());
    const world::BlockCoord opening{supportedWater->x, supportedWater->y - 1, supportedWater->z};
    assert(flowWorld.setBlock(opening.x, opening.y, opening.z, world::BlockId::Air));
    assert(flowWorld.activeWaterCellCount() > 0);
    for (int i = 0; i < 6; ++i) (void)flowWorld.advanceSimulation(0.12f);
    assert(flowWorld.getBlock(opening.x, opening.y, opening.z) == world::BlockId::Water);

    // A camera ray may nominate a target, but only the animated fist sweep is allowed to confirm
    // damage. The same swing locks to the front block and can never continue into the block behind it.
    world::FrontierWorld swingWorld;
    swingWorld.generate(4242u);
    for (int y = 9; y <= 12; ++y) {
        for (int z = 0; z <= 3; ++z) (void)swingWorld.setBlock(0, y, z, world::BlockId::Air, false);
    }
    (void)swingWorld.setBlock(0, 10, 1, world::BlockId::Stone, false);
    (void)swingWorld.setBlock(0, 10, 2, world::BlockId::Stone, false);
    world::RaycastHit intended;
    intended.hit = true;
    intended.block = {0,10,1};
    intended.adjacent = {0,10,0};
    intended.worldX = 0.5f;
    intended.worldY = 10.5f;
    intended.worldZ = 1.0f;
    intended.microResolved = true;

    game::interaction::MiningSwing swing;
    const game::Vec3 eye{0.5f,10.5f,0.0f};
    const game::Vec3 forward{0,0,1};
    const game::Vec3 right{1,0,0};
    const game::Vec3 up{0,1,0};
    assert(swing.begin(intended, eye, forward, right, up, 0.50f));
    int contacts = 0;
    for (int frame = 0; frame < 24; ++frame) {
        if (const auto contact = swing.update(0.03f, swingWorld, eye, forward, right, up)) {
            ++contacts;
            assert(contact->hit.block == intended.block);
            (void)swingWorld.setBlock(intended.block.x, intended.block.y, intended.block.z, world::BlockId::Air, false);
        }
    }
    assert(contacts == 1);
    assert(swingWorld.getBlock(0,10,2) == world::BlockId::Stone);

    world::FrontierWorld particleWorld;
    particleWorld.generate(7331u);
    game::particles::ParticleSystem particles;
    particles.emitBlockBurst(world::BlockId::Water, {0.5f, 8.0f, 0.5f}, 50u);
    assert(particles.size() == 0);
    particles.emitBlockBurst(world::BlockId::Stone, {0.5f, 8.0f, 0.5f}, 1000u, 1.0f);
    assert(particles.size() == game::particles::ParticleSystem::maxParticles);
    for (const auto& particle : particles.particles()) assert(particle.block == world::BlockId::Stone);
    for (int i = 0; i < 40; ++i) particles.update(0.05f, particleWorld);
    assert(particles.size() == 0);
}
