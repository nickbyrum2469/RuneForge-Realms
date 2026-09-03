#include "TestSuites.h"

#include "app/UiState.h"
#include "core/settings/GameSettings.h"
#include "game/Math.h"
#include "game/character/PlayerBodyRig.h"
#include "game/interaction/MiningSwing.h"
#include "game/mining/MiningCadence.h"
#include "game/mining/MiningSystem.h"
#include "game/particles/ParticleSystem.h"
#include "render/scene/CharacterVoxelOrientation.h"
#include "render/scene/VoxelCharacterBuilder.h"
#include "world/Block.h"
#include "world/FrontierWorld.h"
#include "world/blocks/BlockRegistry.h"
#include "world/generation/TerrainGenerator.h"
#include "world/growth/GrassGrowth.h"

#include <cassert>
#include <cmath>
#include <optional>

void runPolishFoundationTests() {
    using namespace rf;

    app::UiState uiState;
    assert(uiState.screen() == app::UiScreen::Hub);
    assert(!uiState.gameplayInputAllowed());
    assert(!uiState.mouseShouldBeCaptured());
    assert(!uiState.nativeOverlayVisible());

    assert(uiState.enterGameplay());
    assert(uiState.gameplayInputAllowed());
    assert(uiState.mouseShouldBeCaptured());
    assert(!uiState.rendererShouldBePaused());
    assert(!uiState.nativeOverlayVisible());

    assert(uiState.openPause());
    assert(uiState.screen() == app::UiScreen::Pause);
    assert(uiState.rendererShouldBePaused());
    assert(!uiState.gameplayInputAllowed());
    assert(!uiState.mouseShouldBeCaptured());
    assert(uiState.nativeOverlayVisible());

    assert(uiState.openSettings());
    assert(uiState.screen() == app::UiScreen::Settings);
    assert(uiState.settingsReturnScreen() == app::UiScreen::Pause);
    assert(uiState.rendererShouldBePaused());
    assert(uiState.nativeOverlayVisible());
    assert(uiState.closeSettings());
    assert(uiState.screen() == app::UiScreen::Pause);
    assert(uiState.closePause());
    assert(uiState.screen() == app::UiScreen::Gameplay);

    assert(uiState.openInventory());
    assert(uiState.screen() == app::UiScreen::Inventory);
    assert(uiState.rendererShouldBePaused());
    assert(uiState.nativeOverlayVisible());
    assert(!uiState.openPause());
    assert(uiState.closeInventory());
    assert(uiState.screen() == app::UiScreen::Gameplay);

    uiState.returnToHub();
    assert(uiState.openSettings());
    assert(uiState.settingsReturnScreen() == app::UiScreen::Hub);
    assert(uiState.closeSettings());
    assert(uiState.screen() == app::UiScreen::Hub);
    assert(!uiState.nativeOverlayVisible());

    const game::Vec3 basisForward = game::forwardFromAngles(0.65f, 0.42f);
    const game::Vec3 basisRight = game::normalized({basisForward.z, 0.0f, -basisForward.x});
    const game::Vec3 basisUp = game::normalized(game::cross(basisForward, basisRight));
    assert(basisUp.y > 0.0f);
    assert(std::abs(game::dot(basisForward, basisRight)) < 0.0001f);
    assert(std::abs(game::dot(basisForward, basisUp)) < 0.0001f);
    assert(std::abs(game::dot(basisRight, basisUp)) < 0.0001f);

    // The hero may rotate in world space, but every tiny cube must rotate with the actor root rather
    // than keeping terrain/world-axis normals. This specifically protects the 45-degree "diamond body"
    // regression caught in the user's 0.5.3 hardware screenshots.
    const game::Vec3 diagonalForward = game::normalized(game::Vec3{1.0f, 0.0f, 1.0f});
    const auto diagonalPose = game::character::PlayerBodyRig::solve({0.0f, 0.0f, 0.0f}, diagonalForward, false);
    game::character::CharacterAppearance diagonalAppearance{};
    auto diagonalMesh = render::scene::VoxelCharacterBuilder::build(diagonalPose, diagonalAppearance);
    render::scene::orientCharacterVoxels(diagonalMesh, diagonalPose);
    bool sawActorForwardNormal = false;
    bool sawActorRightNormal = false;
    for (const auto& vertex : diagonalMesh.vertices) {
        const game::Vec3 normal{vertex.nx, vertex.ny, vertex.nz};
        sawActorForwardNormal = sawActorForwardNormal || game::dot(normal, diagonalPose.forward) > 0.999f;
        sawActorRightNormal = sawActorRightNormal || game::dot(normal, diagonalPose.right) > 0.999f;
    }
    assert(sawActorForwardNormal);
    assert(sawActorRightNormal);

    // Grass is material silhouette, not a random sparse event. All 8x8 cells exist from world age zero
    // and remain present after aging; age may only make tiny maturity/height changes.
    int grassCells = 0;
    for (int z = 0; z < world::growth::GrassGrowth::nodeResolution; ++z) {
        for (int x = 0; x < world::growth::GrassGrowth::nodeResolution; ++x) {
            const auto initial = world::growth::GrassGrowth::sample(1337u, {4, 10, -3}, x, z, 0.0f);
            const auto aged = world::growth::GrassGrowth::sample(1337u, {4, 10, -3}, x, z, 10000.0f);
            assert(initial.present);
            assert(aged.present);
            assert(initial.height >= 0.060f && initial.height <= 0.0951f);
            assert(aged.height >= 0.060f && aged.height <= 0.0951f);
            ++grassCells;
        }
    }
    assert(grassCells == world::growth::GrassGrowth::nodeResolution * world::growth::GrassGrowth::nodeResolution);

    game::mining::MiningCadence cadence;
    cadence.press();
    assert(cadence.update(0.0f, 0.50f));
    cadence.release();
    cadence.press();
    assert(!cadence.update(0.10f, 0.50f));
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

    world::FrontierWorld swingWorld;
    swingWorld.generate(4242u);
    for (int x = -1; x <= 1; ++x) {
        for (int y = 9; y <= 12; ++y) {
            for (int z = -1; z <= 3; ++z) {
                (void)swingWorld.setBlock(x, y, z, world::BlockId::Air, false);
            }
        }
    }
    (void)swingWorld.setBlock(0, 10, 1, world::BlockId::Stone, false);
    (void)swingWorld.setBlock(0, 10, 2, world::BlockId::Stone, false);
    world::RaycastHit intended;
    intended.hit = true;
    intended.block = {0,10,1};
    intended.adjacent = {0,10,0};
    intended.worldX = 0.5f;
    intended.worldY = 10.55f;
    intended.worldZ = 1.0f;
    intended.microResolved = true;

    game::interaction::MiningSwing swing;
    const game::Vec3 feet{0.5f,9.0f,0.5f};
    const game::Vec3 eye{0.5f,10.62f,0.5f};
    const game::Vec3 forward{0,0,1};
    const game::Vec3 right{1,0,0};
    const game::Vec3 up{0,1,0};
    assert(swing.begin(intended, feet, false, eye, forward, right, up, 0.50f));
    int contacts = 0;
    int firstContactFrame = -1;
    for (int frame = 0; frame < 30; ++frame) {
        if (const auto contact = swing.update(0.025f, swingWorld, feet, false, eye, forward, right, up)) {
            ++contacts;
            if (firstContactFrame < 0) firstContactFrame = frame;
            assert(contact->hit.block == intended.block);
            (void)swingWorld.setBlock(intended.block.x, intended.block.y, intended.block.z, world::BlockId::Air, false);
        }
    }
    assert(contacts == 1);
    assert(firstContactFrame >= 4 && firstContactFrame <= 7); // compact ~30% impact, not the old late punch.
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
