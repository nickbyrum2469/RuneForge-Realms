#include "render/scene/FirstPersonBodyBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace rf::render::scene {
namespace {

using game::Vec3;

constexpr float pi = 3.14159265358979323846f;

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

Vec3 rotateAroundAxis(Vec3 value, Vec3 axis, float radians) noexcept {
    axis = game::normalized(axis);
    if (game::lengthSquared(axis) <= 0.000001f || std::abs(radians) <= 0.000001f) return value;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return value * c + game::cross(axis, value) * s + axis * (game::dot(axis, value) * (1.0f - c));
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

    addOrientedBox(mesh, hand, right, up, forward, 0.135f, 0.115f, 0.120f, skin);
    const Vec3 wrist = hand + right * (sideSign * 0.032f) - up * 0.082f - forward * 0.045f;
    const Vec3 offscreen = wrist + right * (sideSign * 0.075f) - up * 0.180f - forward * 0.085f;
    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i) / 3.0f;
        const Vec3 p = offscreen * (1.0f - t) + wrist * t;
        addOrientedBox(mesh, p, right, up, forward, 0.090f, 0.090f, 0.095f, skin);
    }

    for (int finger = -2; finger <= 1; ++finger) {
        addOrientedBox(mesh,
                       hand + forward * 0.055f + right * (static_cast<float>(finger) * 0.030f + 0.015f),
                       right, up, forward, 0.036f, 0.050f, 0.055f, skin);
    }
    addOrientedBox(mesh, hand + right * (sideSign * 0.078f) - up * 0.018f + forward * 0.010f,
                   right, up, forward, 0.045f, 0.052f, 0.060f, skin);
}

struct AnimatedHandPose {
    Vec3 position{};
    Vec3 right{};
    Vec3 up{};
    Vec3 forward{};
};

AnimatedHandPose animatedRightHand(const FirstPersonViewModelState& state,
                                   Vec3 eye,
                                   Vec3 forward,
                                   Vec3 right,
                                   Vec3 up,
                                   Vec3 rest) noexcept {
    AnimatedHandPose pose{rest, right, up, forward};
    if (!state.swingActive) return pose;

    // RuneForge now follows the compact first-person swing language that makes Minecraft so readable:
    // a fast early inward/downward arc driven by sin(sqrt(progress)*pi), a shorter forward pulse, and
    // a clean return to the exact persistent rest pose. We keep RuneForge's distance-responsive reach,
    // but do not make the hand wind up behind/below the camera or pop in from nowhere.
    const float t = std::clamp(state.swingTime, 0.0f, 1.0f);
    const float root = std::sqrt(t);
    const float arc = std::sin(root * pi);
    const float forwardPulse = std::sin(t * pi);
    const float verticalWave = std::sin(root * pi * 2.0f);
    const float centerWeight = std::clamp(arc * arc * 0.82f, 0.0f, 0.82f);

    const float targetRatio = std::clamp(state.targetDistance / 2.45f, 0.0f, 1.0f);
    const float strikeDepth = 0.50f + targetRatio * 0.36f;
    const Vec3 centerStrike = eye + forward * strikeDepth;

    pose.position = rest * (1.0f - centerWeight) + centerStrike * centerWeight;
    pose.position = pose.position - right * (arc * 0.070f)
                                  + up * (verticalWave * 0.032f)
                                  + forward * (forwardPulse * (0.030f + targetRatio * 0.035f));

    // Rotate the whole hand/item as one camera-space viewmodel. This is the part that gives the swing
    // the familiar quick tool-swipe feel instead of looking like a fist translating on rails.
    const float pitch = -arc * 1.05f;
    const float yaw = -arc * 0.22f;
    pose.forward = rotateAroundAxis(forward, right, pitch);
    pose.up = rotateAroundAxis(up, right, pitch);
    pose.forward = rotateAroundAxis(pose.forward, up, yaw);
    pose.right = rotateAroundAxis(right, up, yaw);
    pose.up = game::normalized(game::cross(pose.forward, pose.right));
    pose.right = game::normalized(pose.right);
    pose.forward = game::normalized(pose.forward);
    if (game::lengthSquared(pose.up) <= 0.000001f) pose.up = up;
    return pose;
}

} // namespace

world::VoxelMesh FirstPersonBodyBuilder::build(const FirstPersonViewModelState& input) {
    world::VoxelMesh mesh;
    Vec3 forward = input.forward;
    Vec3 right = input.right;
    Vec3 up = input.up;
    canonicalizeBasis(forward, right, up);

    const float walk = std::clamp(input.walkAmount, 0.0f, 1.0f);
    const float sideBob = std::sin(input.walkPhase) * 0.010f * walk;
    const float verticalBob = std::abs(std::cos(input.walkPhase)) * 0.010f * walk;

    const Vec3 rightRest = input.eye + forward * 0.405f + right * (0.255f + sideBob) -
                           up * (0.255f + verticalBob);
    const Vec3 leftRest = input.eye + forward * 0.395f - right * (0.255f + sideBob) -
                          up * (0.258f + verticalBob);

    const AnimatedHandPose rightHand = animatedRightHand(input, input.eye, forward, right, up, rightRest);

    if (input.equippedBlock.has_value()) {
        addVoxelHand(mesh, rightHand.position, 1.0f, rightHand.right, rightHand.up, rightHand.forward);
        const Vec3 itemCenter = rightHand.position + rightHand.forward * 0.115f +
                                rightHand.up * 0.014f - rightHand.right * 0.004f;
        addOrientedBox(mesh, itemCenter, rightHand.right, rightHand.up, rightHand.forward,
                       0.175f, 0.175f, 0.175f, heldBlockFaces(*input.equippedBlock));
        return mesh;
    }

    addVoxelHand(mesh, rightHand.position, 1.0f, rightHand.right, rightHand.up, rightHand.forward);
    addVoxelHand(mesh, leftRest, -1.0f, right, up, forward);
    return mesh;
}

} // namespace rf::render::scene
