#include "render/scene/VoxelCharacterBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rf::render::scene {
namespace {

using game::Vec3;
using game::character::ArmPose;
using game::character::GearVisual;
using game::character::LegPose;
using game::character::PlayerBodyPose;

void addQuad(world::VoxelMesh& mesh,
             const std::array<float, 3>& p0,
             const std::array<float, 3>& p1,
             const std::array<float, 3>& p2,
             const std::array<float, 3>& p3,
             const std::array<float, 3>& normal,
             world::SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const std::uint32_t packed = world::packMaterial(material);
    auto vertex = [&](const std::array<float, 3>& p) {
        return world::MeshVertex{p[0], p[1], p[2], normal[0], normal[1], normal[2], packed};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addBox(world::VoxelMesh& mesh, Vec3 center, float sx, float sy, float sz,
            world::SurfaceMaterial material) {
    const float x0 = center.x - sx * 0.5f;
    const float x1 = center.x + sx * 0.5f;
    const float y0 = center.y - sy * 0.5f;
    const float y1 = center.y + sy * 0.5f;
    const float z0 = center.z - sz * 0.5f;
    const float z1 = center.z + sz * 0.5f;
    addQuad(mesh, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1}, material);
    addQuad(mesh, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {0, 0, -1}, material);
    addQuad(mesh, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {1, 0, 0}, material);
    addQuad(mesh, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1, 0, 0}, material);
    addQuad(mesh, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}, {0, 1, 0}, material);
    addQuad(mesh, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0}, material);
}

void addPixel(world::VoxelMesh& mesh, Vec3 center, float size, world::SurfaceMaterial material) {
    addBox(mesh, center, size, size, size, material);
}

Vec3 localPoint(Vec3 origin, const PlayerBodyPose& pose, float right, float up, float forward) noexcept {
    return origin + pose.right * right + pose.up * up + pose.forward * forward;
}

Vec3 safeDirection(Vec3 from, Vec3 to, Vec3 fallback) noexcept {
    Vec3 result = game::normalized(to - from);
    return game::lengthSquared(result) > 0.000001f ? result : fallback;
}

void addVoxelChain(world::VoxelMesh& mesh,
                   Vec3 from,
                   Vec3 to,
                   int count,
                   float size,
                   world::SurfaceMaterial material) {
    for (int i = 0; i < count; ++i) {
        const float t = count <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(count - 1);
        addPixel(mesh, from * (1.0f - t) + to * t, size, material);
    }
}

world::SurfaceMaterial gearMaterial(GearVisual visual) noexcept {
    switch (visual) {
        case GearVisual::Cloth: return world::SurfaceMaterial::CharacterBlueCloth;
        case GearVisual::Leather: return world::SurfaceMaterial::CharacterLeather;
        case GearVisual::Iron: return world::SurfaceMaterial::CharacterMetal;
        case GearVisual::None: break;
    }
    return world::SurfaceMaterial::CharacterSkin;
}

void addTorso(world::VoxelMesh& mesh, const PlayerBodyPose& pose, world::SurfaceMaterial material,
              float expansion = 0.0f) {
    const Vec3 torsoAxis = game::normalized(pose.chest - pose.pelvis);
    constexpr int layers = 7;
    constexpr float pixel = 0.074f;
    for (int layer = 0; layer < layers; ++layer) {
        const float t = static_cast<float>(layer) / static_cast<float>(layers - 1);
        const Vec3 center = pose.pelvis * (1.0f - t) + pose.chest * t;
        const int halfWidth = layer < 2 ? 2 : 3;
        constexpr int halfDepth = 1;
        for (int x = -halfWidth; x <= halfWidth; ++x) {
            for (int z = -halfDepth; z <= halfDepth; ++z) {
                const bool surface = std::abs(x) == halfWidth || std::abs(z) == halfDepth ||
                                     layer == 0 || layer == layers - 1;
                if (!surface) continue;
                const Vec3 p = center + pose.right * (static_cast<float>(x) * (pixel + expansion * 0.12f)) +
                               torsoAxis * 0.0f + pose.forward * (static_cast<float>(z) * (pixel + expansion * 0.10f));
                addPixel(mesh, p, pixel + expansion, material);
            }
        }
    }

    if (expansion <= 0.0001f && material == world::SurfaceMaterial::CharacterSkin) {
        // Small stepped protrusions give the bare base body the same readable chest/abdominal planes
        // as the reference sculpt without turning the silhouette into a smooth capsule.
        const Vec3 upper = pose.pelvis * 0.30f + pose.chest * 0.70f;
        addPixel(mesh, upper + pose.right * 0.115f + pose.forward * 0.125f, 0.066f, material);
        addPixel(mesh, upper - pose.right * 0.115f + pose.forward * 0.125f, 0.066f, material);
        const Vec3 middle = pose.pelvis * 0.52f + pose.chest * 0.48f;
        addPixel(mesh, middle + pose.forward * 0.124f, 0.058f, material);
        addPixel(mesh, middle - pose.up * 0.085f + pose.forward * 0.122f, 0.054f, material);
        addPixel(mesh, upper + pose.right * 0.13f - pose.forward * 0.125f, 0.060f, material);
        addPixel(mesh, upper - pose.right * 0.13f - pose.forward * 0.125f, 0.060f, material);
    }
}

void addHand(world::VoxelMesh& mesh, const PlayerBodyPose& body, const ArmPose& arm,
             world::SurfaceMaterial material, float expansion = 0.0f) {
    const Vec3 handDirection = safeDirection(arm.wrist, arm.hand, body.forward);
    Vec3 side = game::normalized(game::cross(body.up, handDirection));
    if (game::lengthSquared(side) <= 0.000001f) side = body.right;
    const float pixel = 0.070f + expansion;
    addPixel(mesh, arm.hand + side * 0.036f, pixel, material);
    addPixel(mesh, arm.hand - side * 0.036f, pixel, material);
    addPixel(mesh, arm.hand - body.up * 0.052f + side * 0.032f, pixel * 0.92f, material);
    addPixel(mesh, arm.hand - body.up * 0.052f - side * 0.032f, pixel * 0.92f, material);
    addPixel(mesh, arm.hand + handDirection * 0.060f + side * 0.028f, pixel * 0.74f, material);
    addPixel(mesh, arm.hand + handDirection * 0.060f - side * 0.028f, pixel * 0.74f, material);
}

void addArm(world::VoxelMesh& mesh, const PlayerBodyPose& body, const ArmPose& arm,
            world::SurfaceMaterial material, float expansion = 0.0f) {
    const float upperSize = 0.112f + expansion;
    const float foreSize = 0.102f + expansion;
    addVoxelChain(mesh, arm.shoulder, arm.elbow, 5, upperSize, material);
    addVoxelChain(mesh, arm.elbow, arm.wrist, 5, foreSize, material);
    addPixel(mesh, arm.shoulder, upperSize * 1.08f, material);
    addPixel(mesh, arm.elbow, foreSize * 1.08f, material);
    addHand(mesh, body, arm, material, expansion * 0.55f);
}

void addLeg(world::VoxelMesh& mesh, const PlayerBodyPose& body, const LegPose& leg,
            world::SurfaceMaterial material, float expansion = 0.0f) {
    addVoxelChain(mesh, leg.hip, leg.knee, 6, 0.132f + expansion, material);
    addVoxelChain(mesh, leg.knee, leg.ankle, 6, 0.116f + expansion, material);
    addPixel(mesh, leg.knee, 0.126f + expansion, material);

    const Vec3 footDirection = safeDirection(leg.ankle, leg.foot, body.forward);
    const Vec3 toe = leg.foot + footDirection * 0.055f;
    addPixel(mesh, leg.ankle, 0.120f + expansion, material);
    addPixel(mesh, leg.foot, 0.132f + expansion, material);
    addPixel(mesh, toe + body.right * 0.045f, 0.094f + expansion * 0.7f, material);
    addPixel(mesh, toe, 0.096f + expansion * 0.7f, material);
    addPixel(mesh, toe - body.right * 0.045f, 0.094f + expansion * 0.7f, material);
}

void addLoincloth(world::VoxelMesh& mesh, const PlayerBodyPose& pose) {
    constexpr float beltPixel = 0.062f;
    const Vec3 beltCenter = pose.pelvis + pose.up * 0.035f;
    for (int x = -3; x <= 3; ++x) {
        addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(x) * 0.058f, 0.0f, 0.137f),
                 beltPixel, world::SurfaceMaterial::CharacterLeather);
        addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(x) * 0.058f, 0.0f, -0.137f),
                 beltPixel, world::SurfaceMaterial::CharacterLeather);
    }
    for (int z = -1; z <= 1; ++z) {
        addPixel(mesh, localPoint(beltCenter, pose, 0.205f, 0.0f, static_cast<float>(z) * 0.064f),
                 beltPixel, world::SurfaceMaterial::CharacterLeather);
        addPixel(mesh, localPoint(beltCenter, pose, -0.205f, 0.0f, static_cast<float>(z) * 0.064f),
                 beltPixel, world::SurfaceMaterial::CharacterLeather);
    }

    constexpr float clothPixel = 0.066f;
    for (int row = 0; row < 5; ++row) {
        const int halfWidth = row < 2 ? 2 : (row < 4 ? 1 : 0);
        const float y = -0.045f - static_cast<float>(row) * 0.058f;
        for (int x = -halfWidth; x <= halfWidth; ++x) {
            const float rx = static_cast<float>(x) * 0.056f;
            addPixel(mesh, localPoint(beltCenter, pose, rx, y, 0.153f), clothPixel,
                     world::SurfaceMaterial::CharacterLoincloth);
            addPixel(mesh, localPoint(beltCenter, pose, rx, y, -0.153f), clothPixel,
                     world::SurfaceMaterial::CharacterLoincloth);
        }
    }
    // Side overlap keeps the base character modest from oblique/rear camera angles while still
    // reading as a minimal survival loincloth rather than permanent armor.
    for (int side = -1; side <= 1; side += 2) {
        for (int row = 0; row < 3; ++row) {
            addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(side) * 0.190f,
                                      -0.055f - static_cast<float>(row) * 0.056f, 0.0f),
                     clothPixel, world::SurfaceMaterial::CharacterLoincloth);
        }
    }
}

void addHead(world::VoxelMesh& mesh, const PlayerBodyPose& pose) {
    constexpr float pixel = 0.070f;
    const Vec3 center = pose.head;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            for (int z = -2; z <= 2; ++z) {
                if (std::abs(x) != 2 && std::abs(y) != 2 && std::abs(z) != 2) continue;
                addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * pixel,
                                          static_cast<float>(y) * pixel,
                                          static_cast<float>(z) * pixel),
                         pixel, world::SurfaceMaterial::CharacterSkin);
            }
        }
    }

    // Dark, layered voxel hair: full crown/back mass plus an irregular fringe over the forehead.
    for (int x = -3; x <= 3; ++x) {
        for (int z = -2; z <= 2; ++z) {
            const int irregular = ((x * 17 + z * 11 + 31) & 1);
            addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.061f,
                                      0.190f + static_cast<float>(irregular) * 0.020f,
                                      static_cast<float>(z) * 0.064f),
                     0.068f, world::SurfaceMaterial::CharacterHair);
        }
    }
    for (int y = -1; y <= 2; ++y) {
        for (int x = -3; x <= 3; ++x) {
            addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.061f,
                                      static_cast<float>(y) * 0.062f,
                                      -0.190f),
                     0.068f, world::SurfaceMaterial::CharacterHair);
        }
    }
    for (int side = -1; side <= 1; side += 2) {
        for (int y = -1; y <= 2; ++y) {
            for (int z = -2; z <= 1; ++z) {
                addPixel(mesh, localPoint(center, pose, static_cast<float>(side) * 0.190f,
                                          static_cast<float>(y) * 0.061f,
                                          static_cast<float>(z) * 0.061f),
                         0.066f, world::SurfaceMaterial::CharacterHair);
            }
        }
    }
    const std::array<float, 7> fringeHeights{{0.125f, 0.095f, 0.140f, 0.105f, 0.135f, 0.085f, 0.115f}};
    for (int x = -3; x <= 3; ++x) {
        addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.058f,
                                  fringeHeights[static_cast<std::size_t>(x + 3)],
                                  0.190f),
                 0.064f, world::SurfaceMaterial::CharacterHair);
    }

    const Vec3 face = center + pose.forward * 0.199f + pose.up * 0.025f;
    for (int side = -1; side <= 1; side += 2) {
        const Vec3 eye = face + pose.right * (static_cast<float>(side) * 0.084f);
        addPixel(mesh, eye, 0.058f, world::SurfaceMaterial::CharacterEyeWhite);
        addPixel(mesh, eye + pose.forward * 0.035f, 0.031f, world::SurfaceMaterial::CharacterEyeBlue);
        addPixel(mesh, eye + pose.up * 0.070f + pose.forward * 0.018f, 0.045f,
                 world::SurfaceMaterial::CharacterHair);
    }
    addPixel(mesh, center + pose.forward * 0.211f - pose.up * 0.025f, 0.043f,
             world::SurfaceMaterial::CharacterSkin);
}

void addHelmet(world::VoxelMesh& mesh, const PlayerBodyPose& pose, world::SurfaceMaterial material) {
    constexpr float pixel = 0.078f;
    for (int x = -3; x <= 3; ++x) {
        for (int z = -2; z <= 2; ++z) {
            addPixel(mesh, localPoint(pose.head, pose, static_cast<float>(x) * 0.067f, 0.220f,
                                      static_cast<float>(z) * 0.067f),
                     pixel, material);
        }
    }
    for (int side = -1; side <= 1; side += 2) {
        for (int y = -1; y <= 2; ++y) {
            addPixel(mesh, localPoint(pose.head, pose, static_cast<float>(side) * 0.220f,
                                      static_cast<float>(y) * 0.070f, -0.025f),
                     pixel, material);
        }
    }
}

void addBackGear(world::VoxelMesh& mesh, const PlayerBodyPose& pose, world::SurfaceMaterial material) {
    const Vec3 axis = game::normalized(pose.chest - pose.pelvis);
    for (int row = 0; row < 7; ++row) {
        const float t = static_cast<float>(row) / 6.0f;
        const Vec3 center = pose.chest * (1.0f - t) + pose.pelvis * t - pose.forward * 0.180f - axis * 0.015f;
        const int halfWidth = row < 5 ? 3 : 2;
        for (int x = -halfWidth; x <= halfWidth; ++x) {
            addPixel(mesh, center + pose.right * (static_cast<float>(x) * 0.070f), 0.068f, material);
        }
    }
}

} // namespace

world::VoxelMesh VoxelCharacterBuilder::build(const game::character::PlayerBodyPose& pose,
                                              const game::character::CharacterAppearance& appearance,
                                              CharacterBuildOptions options) {
    world::VoxelMesh mesh;

    addTorso(mesh, pose, world::SurfaceMaterial::CharacterSkin);
    addArm(mesh, pose, pose.rightArm, world::SurfaceMaterial::CharacterSkin);
    addArm(mesh, pose, pose.leftArm, world::SurfaceMaterial::CharacterSkin);
    addLeg(mesh, pose, pose.rightLeg, world::SurfaceMaterial::CharacterSkin);
    addLeg(mesh, pose, pose.leftLeg, world::SurfaceMaterial::CharacterSkin);

    if (appearance.showLoincloth) addLoincloth(mesh, pose);
    if (options.includeHead) addHead(mesh, pose);

    if (appearance.chest != GearVisual::None) {
        addTorso(mesh, pose, gearMaterial(appearance.chest), 0.026f);
    }
    if (appearance.hands != GearVisual::None) {
        const auto material = gearMaterial(appearance.hands);
        addHand(mesh, pose, pose.rightArm, material, 0.020f);
        addHand(mesh, pose, pose.leftArm, material, 0.020f);
    }
    if (appearance.legs != GearVisual::None) {
        const auto material = gearMaterial(appearance.legs);
        addVoxelChain(mesh, pose.rightLeg.hip, pose.rightLeg.knee, 6, 0.151f, material);
        addVoxelChain(mesh, pose.leftLeg.hip, pose.leftLeg.knee, 6, 0.151f, material);
        addVoxelChain(mesh, pose.rightLeg.knee, pose.rightLeg.ankle, 6, 0.133f, material);
        addVoxelChain(mesh, pose.leftLeg.knee, pose.leftLeg.ankle, 6, 0.133f, material);
    }
    if (appearance.feet != GearVisual::None) {
        const auto material = gearMaterial(appearance.feet);
        addPixel(mesh, pose.rightLeg.ankle, 0.144f, material);
        addPixel(mesh, pose.rightLeg.foot, 0.156f, material);
        addPixel(mesh, pose.leftLeg.ankle, 0.144f, material);
        addPixel(mesh, pose.leftLeg.foot, 0.156f, material);
    }
    if (options.includeHead && appearance.head != GearVisual::None) {
        addHelmet(mesh, pose, gearMaterial(appearance.head));
    }
    if (appearance.back != GearVisual::None) {
        addBackGear(mesh, pose, gearMaterial(appearance.back));
    }

    return mesh;
}

} // namespace rf::render::scene
