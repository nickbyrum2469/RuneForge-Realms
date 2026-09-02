#include "render/scene/FirstPersonBodyBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace rf::render::scene {
namespace {

using game::Vec3;

void canonicalizeBasis(Vec3& forward, Vec3& right, Vec3& up) noexcept {
    forward = game::normalized(forward);
    if (game::lengthSquared(forward) <= 0.000001f) forward = {0.0f, 0.0f, 1.0f};

    right = game::normalized(right - forward * game::dot(right, forward));
    if (game::lengthSquared(right) <= 0.000001f) {
        right = game::normalized({forward.z, 0.0f, -forward.x});
        if (game::lengthSquared(right) <= 0.000001f) right = {1.0f, 0.0f, 0.0f};
    }

    up = game::normalized(game::cross(forward, right));
    if (game::lengthSquared(up) <= 0.000001f) up = {0.0f, 1.0f, 0.0f};
}

void addQuad(world::VoxelMesh& mesh,
             Vec3 p0,
             Vec3 p1,
             Vec3 p2,
             Vec3 p3,
             Vec3 normal,
             world::SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const std::uint32_t packed = world::packMaterial(material);
    auto vertex = [&](Vec3 p) {
        return world::MeshVertex{p.x, p.y, p.z, normal.x, normal.y, normal.z, packed};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

struct FaceMaterials {
    world::SurfaceMaterial front{world::SurfaceMaterial::CharacterSkin};
    world::SurfaceMaterial back{world::SurfaceMaterial::CharacterSkin};
    world::SurfaceMaterial right{world::SurfaceMaterial::CharacterSkin};
    world::SurfaceMaterial left{world::SurfaceMaterial::CharacterSkin};
    world::SurfaceMaterial top{world::SurfaceMaterial::CharacterSkin};
    world::SurfaceMaterial bottom{world::SurfaceMaterial::CharacterSkin};
};

void addOrientedBox(world::VoxelMesh& mesh,
                    Vec3 center,
                    Vec3 right,
                    Vec3 up,
                    Vec3 forward,
                    float sx,
                    float sy,
                    float sz,
                    FaceMaterials materials) {
    const Vec3 r = right * (sx * 0.5f);
    const Vec3 u = up * (sy * 0.5f);
    const Vec3 f = forward * (sz * 0.5f);

    addQuad(mesh, center - r - u + f, center + r - u + f, center + r + u + f, center - r + u + f,
            forward, materials.front);
    addQuad(mesh, center + r - u - f, center - r - u - f, center - r + u - f, center + r + u - f,
            forward * -1.0f, materials.back);
    addQuad(mesh, center + r - u + f, center + r - u - f, center + r + u - f, center + r + u + f,
            right, materials.right);
    addQuad(mesh, center - r - u - f, center - r - u + f, center - r + u + f, center - r + u - f,
            right * -1.0f, materials.left);
    addQuad(mesh, center - r + u + f, center + r + u + f, center + r + u - f, center - r + u - f,
            up, materials.top);
    addQuad(mesh, center - r - u - f, center + r - u - f, center + r - u + f, center - r - u + f,
            up * -1.0f, materials.bottom);
}

FaceMaterials skinFaces() noexcept {
    FaceMaterials faces;
    faces.front = faces.back = faces.right = faces.left = faces.top = faces.bottom =
        world::SurfaceMaterial::CharacterSkin;
    return faces;
}

FaceMaterials heldBlockFaces(world::BlockId block) noexcept {
    FaceMaterials faces;
    switch (block) {
        case world::BlockId::Grass:
            faces.front = faces.back = faces.right = faces.left = world::SurfaceMaterial::GrassSide;
            faces.top = world::SurfaceMaterial::GrassTop;
            faces.bottom = world::SurfaceMaterial::Dirt;
            break;
        case world::BlockId::Dirt:
            faces.front = faces.back = faces.right = faces.left = faces.top = faces.bottom =
                world::SurfaceMaterial::Dirt;
            break;
        case world::BlockId::Stone:
            faces.front = faces.back = faces.right = faces.left = faces.top = faces.bottom =
                world::SurfaceMaterial::Stone;
            break;
        case world::BlockId::Wood:
            faces.front = faces.back = faces.right = faces.left = world::SurfaceMaterial::WoodBark;
            faces.top = faces.bottom = world::SurfaceMaterial::WoodCut;
            break;
        case world::BlockId::Leaves:
            faces.front = faces.back = faces.right = faces.left = faces.top = faces.bottom =
                world::SurfaceMaterial::Leaves;
            break;
        case world::BlockId::Water:
            faces.front = faces.back = faces.right = faces.left = faces.top = faces.bottom =
                world::SurfaceMaterial::Water;
            break;
        case world::BlockId::Air:
            break;
    }
    return faces;
}

void addVoxelHand(world::VoxelMesh& mesh,
                  Vec3 hand,
                  float sideSign,
                  Vec3 right,
                  Vec3 up,
                  Vec3 forward) {
    const FaceMaterials skin = skinFaces();

    // Palm is compact and intentionally sits mostly below the center of the viewport. The wrist
    // chain continues down/out of frame so the hand never materializes from nowhere when a swing begins.
    addOrientedBox(mesh, hand, right, up, forward, 0.135f, 0.115f, 0.120f, skin);
    const Vec3 wrist = hand + right * (sideSign * 0.032f) - up * 0.082f - forward * 0.045f;
    const Vec3 offscreen = wrist + right * (sideSign * 0.075f) - up * 0.180f - forward * 0.085f;
    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i) / 3.0f;
        const Vec3 p = offscreen * (1.0f - t) + wrist * t;
        addOrientedBox(mesh, p, right, up, forward, 0.090f, 0.090f, 0.095f, skin);
    }

    // Four small finger voxels preserve the reference character's readable block-built hand.
    for (int finger = -2; finger <= 1; ++finger) {
        addOrientedBox(mesh,
                       hand + forward * 0.055f + right * (static_cast<float>(finger) * 0.030f + 0.015f),
                       right, up, forward, 0.036f, 0.050f, 0.055f, skin);
    }
    addOrientedBox(mesh, hand + right * (sideSign * 0.078f) - up * 0.018f + forward * 0.010f,
                   right, up, forward, 0.045f, 0.052f, 0.060f, skin);
}

Vec3 animatedRightHand(const FirstPersonViewModelState& state,
                       Vec3 forward,
                       Vec3 right,
                       Vec3 up,
                       Vec3 rest) noexcept {
    if (!state.swingActive) return rest;

    const float t = std::clamp(state.swingTime, 0.0f, 1.0f);
    const float accelerated = std::sqrt(t);
    const float primaryArc = std::sin(accelerated * 3.14159265f);
    const float lateralArc = std::sin(accelerated * 6.28318530f);
    const float targetRatio = std::clamp(state.targetDistance / 2.45f, 0.0f, 1.0f);

    // Minecraft's readable hand motion comes from a compact screen-space arc rather than throwing
    // the hand all the way to the raycast point. Keep the RuneForge fist attached to its persistent
    // lower-right rest pose, sweep it down/inward, add only a short forward follow-through, then let
    // the sqrt-shaped phase snap naturally back to rest. Target distance may slightly deepen the
    // follow-through, but it can never turn the animation into the old telescoping center-screen jab.
    constexpr float inwardSweep = 0.145f;
    constexpr float downwardSweep = 0.105f;
    constexpr float baseForwardSweep = 0.070f;
    constexpr float targetForwardSweep = 0.035f;
    constexpr float lateralCurl = 0.030f;
    static_assert(inwardSweep < 0.18f && downwardSweep < 0.13f &&
                  baseForwardSweep + targetForwardSweep < 0.12f);

    const float forwardSweep = baseForwardSweep + targetForwardSweep * targetRatio;
    return rest - right * (primaryArc * inwardSweep)
                - up * (primaryArc * downwardSweep)
                + forward * (primaryArc * forwardSweep)
                + right * (lateralArc * lateralCurl);
}

} // namespace

world::VoxelMesh FirstPersonBodyBuilder::build(const FirstPersonViewModelState& input) {
    world::VoxelMesh mesh;
    Vec3 forward = input.forward;
    Vec3 right = input.right;
    Vec3 up = input.up;
    canonicalizeBasis(forward, right, up);

    const float walk = std::clamp(input.walkAmount, 0.0f, 1.0f);
    const float sideBob = std::sin(input.walkPhase) * 0.012f * walk;
    const float verticalBob = std::abs(std::cos(input.walkPhase)) * 0.014f * walk;

    const Vec3 rightRest = input.eye + forward * 0.405f + right * (0.255f + sideBob) -
                           up * (0.255f + verticalBob);
    const Vec3 leftRest = input.eye + forward * 0.395f - right * (0.255f + sideBob) -
                          up * (0.258f + verticalBob);

    const Vec3 rightHand = animatedRightHand(input, forward, right, up, rightRest);

    if (input.equippedBlock.has_value()) {
        // Equipped view: only the dominant hand is shown. The held item is camera-space too, so its
        // voxel faces turn with the player's view instead of visually counter-rotating against it.
        addVoxelHand(mesh, rightHand, 1.0f, right, up, forward);
        const Vec3 itemCenter = rightHand + forward * 0.115f + up * 0.014f - right * 0.004f;
        addOrientedBox(mesh, itemCenter, right, up, forward, 0.175f, 0.175f, 0.175f,
                       heldBlockFaces(*input.equippedBlock));
        return mesh;
    }

    // Empty hands: both hands remain subtly visible at the bottom of the frame during idle/walking.
    // Only the dominant hand leaves that rest pose when attacking; the left never pops in/out.
    addVoxelHand(mesh, rightHand, 1.0f, right, up, forward);
    addVoxelHand(mesh, leftRest, -1.0f, right, up, forward);
    return mesh;
}

} // namespace rf::render::scene