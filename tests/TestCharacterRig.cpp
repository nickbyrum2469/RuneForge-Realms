#include "TestSuites.h"

#include "game/Math.h"
#include "game/PlayerController.h"
#include "game/character/CharacterAppearance.h"
#include "game/character/PlayerBodyRig.h"
#include "game/interaction/MiningSwing.h"
#include "render/scene/VoxelCharacterBuilder.h"
#include "world/Block.h"
#include "world/FrontierWorld.h"

#include <cassert>
#include <cmath>
#include <cstdint>

namespace {

float distance(rf::game::Vec3 a, rf::game::Vec3 b) {
    const auto delta = b - a;
    return std::sqrt(rf::game::lengthSquared(delta));
}

bool hasMaterial(const rf::world::VoxelMesh& mesh, rf::world::SurfaceMaterial material) {
    const std::uint32_t wanted = static_cast<std::uint32_t>(material);
    for (const auto& vertex : mesh.vertices) {
        if ((vertex.material & rf::world::surfaceMaterialMask) == wanted) return true;
    }
    return false;
}

void assertFixedArm(const rf::game::character::ArmPose& arm) {
    using Rig = rf::game::character::PlayerBodyRig;
    assert(std::abs(distance(arm.shoulder, arm.elbow) - Rig::upperArmLength) < 0.0015f);
    assert(std::abs(distance(arm.elbow, arm.wrist) - Rig::forearmLength) < 0.0015f);
}

void assertFixedLeg(const rf::game::character::LegPose& leg) {
    using Rig = rf::game::character::PlayerBodyRig;
    assert(std::abs(distance(leg.hip, leg.knee) - Rig::thighLength) < 0.0015f);
    assert(std::abs(distance(leg.knee, leg.ankle) - Rig::shinLength) < 0.0015f);
}

} // namespace

void runCharacterRigTests() {
    using namespace rf;

    // The canonical body is a fixed-length hierarchy. Far-away animation targets may rotate and
    // bend joints, but they are never allowed to telescope the arm or leg segments.
    game::PlayerController cameraPlayer;
    cameraPlayer.spawn({0.5f, 9.0f, 0.5f}, 0.0f, 0.0f);
    assert(cameraPlayer.cameraMode() == game::CameraMode::FirstPerson);
    cameraPlayer.toggleCameraMode();
    assert(cameraPlayer.cameraMode() == game::CameraMode::ThirdPerson);
    cameraPlayer.toggleCameraMode();
    assert(cameraPlayer.cameraMode() == game::CameraMode::FirstPerson);

    const game::Vec3 feet{0.5f, 9.0f, 0.5f};
    const game::Vec3 forward{0.0f, 0.0f, 1.0f};
    auto body = game::character::PlayerBodyRig::solve(feet, forward, false);
    assertFixedArm(body.rightArm);
    assertFixedArm(body.leftArm);
    assertFixedLeg(body.rightLeg);
    assertFixedLeg(body.leftLeg);

    const game::Vec3 impossibleHand{6.0f, 12.0f, 7.0f};
    body = game::character::PlayerBodyRig::solve(feet, forward, false, &impossibleHand);
    assertFixedArm(body.rightArm);
    assert(distance(body.rightArm.shoulder, body.rightArm.wrist) <=
           game::character::PlayerBodyRig::upperArmLength +
           game::character::PlayerBodyRig::forearmLength + 0.001f);

    const auto crouched = game::character::PlayerBodyRig::solve(feet, forward, true);
    assertFixedArm(crouched.rightArm);
    assertFixedArm(crouched.leftArm);
    assertFixedLeg(crouched.rightLeg);
    assertFixedLeg(crouched.leftLeg);
    assert(crouched.pelvis.y < body.pelvis.y);

    // The permanent hero mesh is bare voxel skin plus hair/eyes and a minimal loincloth. Cloth,
    // armor and future outfit pieces are overlays, not geometry baked into the base character.
    const auto fullPose = game::character::PlayerBodyRig::solve({0.0f, 0.0f, 0.0f}, forward, false);
    const game::character::CharacterAppearance baseAppearance{};
    const auto baseMesh = render::scene::VoxelCharacterBuilder::build(fullPose, baseAppearance);
    assert(!baseMesh.empty());
    assert(hasMaterial(baseMesh, world::SurfaceMaterial::CharacterSkin));
    assert(hasMaterial(baseMesh, world::SurfaceMaterial::CharacterHair));
    assert(hasMaterial(baseMesh, world::SurfaceMaterial::CharacterEyeWhite));
    assert(hasMaterial(baseMesh, world::SurfaceMaterial::CharacterEyeBlue));
    assert(hasMaterial(baseMesh, world::SurfaceMaterial::CharacterLoincloth));
    assert(!hasMaterial(baseMesh, world::SurfaceMaterial::CharacterBlueCloth));
    assert(!hasMaterial(baseMesh, world::SurfaceMaterial::CharacterMetal));

    auto gearedAppearance = baseAppearance;
    gearedAppearance.chest = game::character::GearVisual::Cloth;
    gearedAppearance.hands = game::character::GearVisual::Iron;
    const auto gearedMesh = render::scene::VoxelCharacterBuilder::build(fullPose, gearedAppearance);
    assert(hasMaterial(gearedMesh, world::SurfaceMaterial::CharacterBlueCloth));
    assert(hasMaterial(gearedMesh, world::SurfaceMaterial::CharacterMetal));

    world::FrontierWorld swingWorld;
    swingWorld.generate(4242u);
    for (int x = -2; x <= 2; ++x) {
        for (int y = 8; y <= 12; ++y) {
            for (int z = -1; z <= 3; ++z) {
                (void)swingWorld.setBlock(x, y, z, world::BlockId::Air, false);
            }
        }
    }

    const game::Vec3 eye{0.5f, 10.62f, 0.5f};
    const game::Vec3 right{1.0f, 0.0f, 0.0f};
    const game::Vec3 up{0.0f, 1.0f, 0.0f};

    // LMB must animate even when the camera nominates nothing. Every frame of that miss still uses
    // exactly the same fixed-length physical arm that the renderer consumes.
    game::interaction::MiningSwing airSwing;
    const world::RaycastHit miss{};
    assert(airSwing.begin(miss, feet, false, eye, forward, right, up, 0.46f));
    assert(!airSwing.lockedBlock().has_value());
    int airContacts = 0;
    for (int frame = 0; frame < 28; ++frame) {
        if (airSwing.active()) assertFixedArm(airSwing.pose().rightArm);
        if (airSwing.update(0.025f, swingWorld, feet, false, eye, forward, right, up)) ++airContacts;
        assertFixedArm(airSwing.pose().rightArm);
    }
    assert(airContacts == 0);
    assert(!airSwing.active());

    // Camera nomination only influences intent. The first solid voxel reached by the swept fist is
    // the one physical contact for this swing; deleting it cannot tunnel into a block behind it.
    // Reach test: front voxel is farther than the literal arm chain but still comfortably interactive.
    (void)swingWorld.setBlock(0, 10, 2, world::BlockId::Stone, false);
    (void)swingWorld.setBlock(0, 10, 3, world::BlockId::Stone, false);
    world::RaycastHit intended;
    intended.hit = true;
    intended.block = {0, 10, 2};
    intended.adjacent = {0, 10, 1};
    intended.worldX = 0.5f;
    intended.worldY = 10.55f;
    intended.worldZ = 2.0f;
    intended.microResolved = true;

    game::interaction::MiningSwing hitSwing;
    assert(hitSwing.begin(intended, feet, false, eye, forward, right, up, 0.50f));
    int contacts = 0;
    for (int frame = 0; frame < 30; ++frame) {
        assertFixedArm(hitSwing.pose().rightArm);
        if (const auto contact = hitSwing.update(0.025f, swingWorld, feet, false, eye, forward, right, up)) {
            ++contacts;
            assert((contact->hit.block == world::BlockCoord{0, 10, 2}));
            (void)swingWorld.setBlock(contact->hit.block.x, contact->hit.block.y, contact->hit.block.z,
                                      world::BlockId::Air, false);
        }
    }
    assert(contacts == 1);
    assert(swingWorld.getBlock(0, 10, 3) == world::BlockId::Stone);
}
