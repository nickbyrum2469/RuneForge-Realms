#include "TestSuites.h"

#include "game/PlayerController.h"
#include "game/character/CharacterAppearance.h"
#include "game/character/PlayerBodyRig.h"
#include "render/scene/CharacterVoxelOrientation.h"
#include "render/scene/VoxelCharacterBuilder.h"
#include "world/Block.h"
#include "world/FrontierWorld.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

void runCharacterVoxelOrientationRegression() {
    using namespace rf;

    const game::Vec3 diagonalForward = game::normalized(game::Vec3{1.0f, 0.0f, 1.0f});
    const auto pose = game::character::PlayerBodyRig::solve({0.0f, 0.0f, 0.0f}, diagonalForward, false);
    auto mesh = render::scene::VoxelCharacterBuilder::build(pose, game::character::CharacterAppearance{});

    constexpr std::size_t verticesPerBox = 24;
    assert(!mesh.empty());
    assert(mesh.vertices.size() % verticesPerBox == 0);

    game::Vec3 centerBefore{};
    for (std::size_t i = 0; i < verticesPerBox; ++i) {
        centerBefore = centerBefore + game::Vec3{mesh.vertices[i].x, mesh.vertices[i].y, mesh.vertices[i].z};
    }
    centerBefore = centerBefore * (1.0f / static_cast<float>(verticesPerBox));

    // VoxelCharacterBuilder still emits the individual box faces on world XYZ. This is exactly the
    // real-hardware 45-degree "diamond body" failure: the body centers turn, but each tiny cube does not.
    const game::Vec3 normalBefore{mesh.vertices[0].nx, mesh.vertices[0].ny, mesh.vertices[0].nz};
    assert(std::abs(game::dot(normalBefore, pose.forward)) < 0.99f);

    render::scene::orientCharacterVoxels(mesh, pose);

    game::Vec3 centerAfter{};
    for (std::size_t i = 0; i < verticesPerBox; ++i) {
        centerAfter = centerAfter + game::Vec3{mesh.vertices[i].x, mesh.vertices[i].y, mesh.vertices[i].z};
    }
    centerAfter = centerAfter * (1.0f / static_cast<float>(verticesPerBox));

    // Reorientation may rotate only the micro-cube basis: the articulated body position must not move.
    assert(game::lengthSquared(centerAfter - centerBefore) < 0.0000001f);

    const game::Vec3 normalAfter{mesh.vertices[0].nx, mesh.vertices[0].ny, mesh.vertices[0].nz};
    assert(game::dot(normalAfter, pose.forward) > 0.999f);
    assert(std::abs(normalAfter.x) > 0.70f);
    assert(std::abs(normalAfter.z) > 0.70f);
}

void runCollisionResolvedLocomotionRegression() {
    using namespace rf;

    world::FrontierWorld world;
    world.generate(424242u);
    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 4; ++z) {
            (void)world.setBlock(x, 8, z, world::BlockId::Stone, false);
            for (int y = 9; y <= 12; ++y) {
                (void)world.setBlock(x, y, z, world::BlockId::Air, false);
            }
        }
    }

    // A wall directly in front allows one small approach step, then collision must stop actual travel
    // even while forward input remains held. Character gait/viewmodel bob key off these metrics, so a
    // blocked player cannot treadmill-walk in place just because requested velocity is still nonzero.
    (void)world.setBlock(0, 9, 1, world::BlockId::Stone, false);
    (void)world.setBlock(0, 10, 1, world::BlockId::Stone, false);

    game::PlayerController player;
    player.spawn({0.5f, 9.0f, 0.5f}, 0.0f, 0.0f);
    player.setControl(game::MoveControl::Forward, true);
    for (int i = 0; i < 6; ++i) player.update(0.020f, world);

    const float blockedDistance = player.horizontalTravelDistance();
    assert(blockedDistance > 0.05f);
    assert(blockedDistance < 0.20f);
    assert(player.actualHorizontalSpeed() < 0.001f);

    for (int i = 0; i < 8; ++i) player.update(0.020f, world);
    assert(std::abs(player.horizontalTravelDistance() - blockedDistance) < 0.0001f);
    assert(player.actualHorizontalSpeed() < 0.001f);

    (void)world.setBlock(0, 9, 1, world::BlockId::Air, false);
    (void)world.setBlock(0, 10, 1, world::BlockId::Air, false);
    player.update(0.020f, world);
    assert(player.horizontalTravelDistance() > blockedDistance + 0.08f);
    assert(player.actualHorizontalSpeed() > 4.7f);

    player.spawn({0.5f, 9.0f, 0.5f}, 0.0f, 0.0f);
    assert(player.horizontalTravelDistance() == 0.0f);
    assert(player.actualHorizontalSpeed() == 0.0f);
}

} // namespace

int main() {
    runRegistryTests();
    runJobTests();
    runWorldTests();
    runPersistenceTests();
    runCullingTests();
    runMicroMiningTests();
    runInventoryGrowthTests();
    runDropTests();
    runPolishFoundationTests();
    runCharacterRigTests();
    runCharacterVoxelOrientationRegression();
    runCollisionResolvedLocomotionRegression();

    std::cout << "RuneForge 0.5.0 commercial-foundation tests passed\n";
    return 0;
}