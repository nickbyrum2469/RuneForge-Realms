#include "TestSuites.h"

#include "game/Math.h"
#include "game/PlayerController.h"
#include "game/character/CharacterAppearance.h"
#include "game/character/PlayerBodyRig.h"
#include "game/interaction/MiningSwing.h"
#include "render/scene/FirstPersonBodyBuilder.h"
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

std::size_t materialVertexCount(const rf::world::VoxelMesh& mesh, rf::world::SurfaceMaterial material) {
    const std::uint32_t wanted = static_cast<std::uint32_t>(material);
    std::size_t count = 0;
    for (const auto& vertex : mesh.vertices) {
        if ((vertex.material & rf::world::surfaceMaterialMask) == wanted) ++count;
    }
    return count;
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

void assertHeroTaper(const rf::game::character::PlayerBodyPose& pose) {
    const float shoulderSpan = distance(pose.rightArm.shoulder, pose.leftArm.shoulder);
    const float hipSpan = distance(pose.rightLeg.hip, pose.leftLeg.hip);
    assert(shoulderSpan > 0.625f);
    assert(hipSpan < 0.275f);
    assert(shoulderSpan / hipSpan > 2.25f);
}

} // namespace

void runCharacterRigTests() {
    using namespace rf;

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
    assertHeroTaper(body);
    assert(body.rightArm.hand.y < body.pelvis.y - 0.035f);
    assert(body.leftArm.hand.y < body.pelvis.y - 0.035f);
    assert(distance(body.rightArm.shoulder, body.leftArm.shoulder) > 0.62f);
    assert(game::dot(body.rightLeg.foot - body.rightLeg.ankle, body.forward) > 0.145f);
    assert(game::dot(body.leftLeg.foot - body.leftLeg.ankle, body.forward) > 0.145f);

    // Third-person locomotion uses phase-driven arm/leg targets, but every articulated segment must
    // remain the same anatomical length. Opposite phases should swap the leading foot and arm.
    game::character::BodyMotionState gaitA;
    gaitA.locomotionAmount = 1.0f;
    gaitA.locomotionPhase = 1.57079632679f;
    gaitA.idlePhase = 0.0f;
    const auto walkingA = game::character::PlayerBodyRig::solve(feet, forward, false, nullptr, nullptr, &gaitA);
    assertFixedArm(walkingA.rightArm);
    assertFixedArm(walkingA.leftArm);
    assertFixedLeg(walkingA.rightLeg);
    assertFixedLeg(walkingA.leftLeg);
    assertHeroTaper(walkingA);
    assert(game::dot(walkingA.rightLeg.ankle - feet, walkingA.forward) >
           game::dot(walkingA.leftLeg.ankle - feet, walkingA.forward) + 0.15f);
    assert(game::dot(walkingA.leftArm.hand - walkingA.pelvis, walkingA.forward) >
           game::dot(walkingA.rightArm.hand - walkingA.pelvis, walkingA.forward) + 0.12f);
    assert(walkingA.rightLeg.ankle.y > walkingA.leftLeg.ankle.y + 0.04f);

    game::character::BodyMotionState gaitB = gaitA;
    gaitB.locomotionPhase = 4.71238898038f;
    const auto walkingB = game::character::PlayerBodyRig::solve(feet, forward, false, nullptr, nullptr, &gaitB);
    assertFixedArm(walkingB.rightArm);
    assertFixedArm(walkingB.leftArm);
    assertFixedLeg(walkingB.rightLeg);
    assertFixedLeg(walkingB.leftLeg);
    assertHeroTaper(walkingB);
    assert(game::dot(walkingB.leftLeg.ankle - feet, walkingB.forward) >
           game::dot(walkingB.rightLeg.ankle - feet, walkingB.forward) + 0.15f);
    assert(game::dot(walkingB.rightArm.hand - walkingB.pelvis, walkingB.forward) >
           game::dot(walkingB.leftArm.hand - walkingB.pelvis, walkingB.forward) + 0.12f);
    assert(walkingB.leftLeg.ankle.y > walkingB.rightLeg.ankle.y + 0.04f);

    game::character::BodyMotionState idleMotion;
    idleMotion.idlePhase = 1.57079632679f;
    const auto breathing = game::character::PlayerBodyRig::solve(feet, forward, false, nullptr, nullptr, &idleMotion);
    assertFixedArm(breathing.rightArm);
    assertFixedArm(breathing.leftArm);
    assertFixedLeg(breathing.rightLeg);
    assertFixedLeg(breathing.leftLeg);
    assertHeroTaper(breathing);
    assert(breathing.pelvis.y > body.pelvis.y + 0.003f);

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
    assertHeroTaper(crouched);
    assert(crouched.pelvis.y < body.pelvis.y);

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
    assert(baseMesh.quadCount > 2200);

    auto gearedAppearance = baseAppearance;
    gearedAppearance.chest = game::character::GearVisual::Cloth;
    gearedAppearance.hands = game::character::GearVisual::Iron;
    const auto gearedMesh = render::scene::VoxelCharacterBuilder::build(fullPose, gearedAppearance);
    assert(hasMaterial(gearedMesh, world::SurfaceMaterial::CharacterBlueCloth));
    assert(hasMaterial(gearedMesh, world::SurfaceMaterial::CharacterMetal));

    // First-person is now a persistent camera-space viewmodel instead of borrowing the world-space
    // body rig. Empty-handed idle has two visible hands; equipping a block leaves only the dominant
    // hand and adds the held block's actual materials.
    render::scene::FirstPersonViewModelState viewModel;
    viewModel.eye = {0.0f, 1.62f, 0.0f};
    viewModel.forward = {0.0f, 0.0f, 1.0f};
    viewModel.right = {1.0f, 0.0f, 0.0f};
    viewModel.up = {0.0f, 1.0f, 0.0f};
    const auto bareHands = render::scene::FirstPersonBodyBuilder::build(viewModel);
    assert(!bareHands.empty());
    assert(hasMaterial(bareHands, world::SurfaceMaterial::CharacterSkin));
    assert(!hasMaterial(bareHands, world::SurfaceMaterial::GrassTop));

    viewModel.equippedBlock = world::BlockId::Grass;
    const auto equippedHand = render::scene::FirstPersonBodyBuilder::build(viewModel);
    assert(hasMaterial(equippedHand, world::SurfaceMaterial::CharacterSkin));
    assert(hasMaterial(equippedHand, world::SurfaceMaterial::GrassTop));
    assert(hasMaterial(equippedHand, world::SurfaceMaterial::GrassSide));
    assert(materialVertexCount(equippedHand, world::SurfaceMaterial::CharacterSkin) <
           materialVertexCount(bareHands, world::SurfaceMaterial::CharacterSkin));

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

    // Empty-air LMB still animates, but an unrelated nearby block may never become a stray off-axis
    // contact. This protects the center-crosshair targeting contract.
    (void)swingWorld.setBlock(1, 10, 1, world::BlockId::Stone, false);
    game::interaction::MiningSwing airSwing;
    const world::RaycastHit miss{};
    assert(airSwing.begin(miss, feet, false, eye, forward, right, up, 0.46f));
    assert(!airSwing.lockedBlock().has_value());
    assert(!airSwing.pose().hasTarget);
    int airContacts = 0;
    for (int frame = 0; frame < 28; ++frame) {
        if (airSwing.active()) assertFixedArm(airSwing.pose().rightArm);
        if (airSwing.update(0.025f, swingWorld, feet, false, eye, forward, right, up)) ++airContacts;
        assertFixedArm(airSwing.pose().rightArm);
    }
    assert(airContacts == 0);
    assert(!airSwing.active());
    (void)swingWorld.setBlock(1, 10, 1, world::BlockId::Air, false);

    // The first solid block selected by the center camera ray is locked for the swing. Impact occurs
    // once at that exact nominated block; deleting it cannot tunnel through to a block behind it.
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
    assert(hitSwing.pose().hasTarget);
    assert(hitSwing.pose().targetDistance > 1.0f);
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