#include "render/scene/FirstPersonBodyBuilder.h"

#include <array>
#include <cstdint>

namespace rf::render::scene {
namespace {

using game::Vec3;

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

void addBox(world::VoxelMesh& mesh, Vec3 center, float size, world::SurfaceMaterial material) {
    const float h = size * 0.5f;
    const float x0 = center.x - h, x1 = center.x + h;
    const float y0 = center.y - h, y1 = center.y + h;
    const float z0 = center.z - h, z1 = center.z + h;
    addQuad(mesh, {x0,y0,z1}, {x1,y0,z1}, {x1,y1,z1}, {x0,y1,z1}, {0,0,1}, material);
    addQuad(mesh, {x1,y0,z0}, {x0,y0,z0}, {x0,y1,z0}, {x1,y1,z0}, {0,0,-1}, material);
    addQuad(mesh, {x1,y0,z1}, {x1,y0,z0}, {x1,y1,z0}, {x1,y1,z1}, {1,0,0}, material);
    addQuad(mesh, {x0,y0,z0}, {x0,y0,z1}, {x0,y1,z1}, {x0,y1,z0}, {-1,0,0}, material);
    addQuad(mesh, {x0,y1,z1}, {x1,y1,z1}, {x1,y1,z0}, {x0,y1,z0}, {0,1,0}, material);
    addQuad(mesh, {x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1}, {x0,y0,z1}, {0,-1,0}, material);
}

void addChain(world::VoxelMesh& mesh,
              Vec3 from,
              Vec3 to,
              int count,
              float size,
              Vec3 side,
              Vec3 up,
              world::SurfaceMaterial material) {
    for (int i = 0; i < count; ++i) {
        const float t = count <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(count - 1);
        const Vec3 center = from * (1.0f - t) + to * t;
        // A compact 2x2 voxel cross-section reads as an arm rather than a string of beads, while
        // keeping the first-person view clear enough that it never becomes a torso/body overlay.
        const float o = size * 0.27f;
        addBox(mesh, center + side * o + up * o, size * 0.58f, material);
        addBox(mesh, center - side * o + up * o, size * 0.58f, material);
        addBox(mesh, center + side * o - up * o, size * 0.58f, material);
        addBox(mesh, center - side * o - up * o, size * 0.58f, material);
    }
}

} // namespace

world::VoxelMesh FirstPersonBodyBuilder::build(const game::character::PlayerBodyPose& pose,
                                               const game::character::CharacterAppearance&) {
    world::VoxelMesh mesh;
    const auto& arm = pose.rightArm;
    constexpr auto skin = world::SurfaceMaterial::CharacterSkin;

    addChain(mesh, arm.shoulder, arm.elbow, 5, 0.125f, pose.right, pose.up, skin);
    addChain(mesh, arm.elbow, arm.wrist, 5, 0.112f, pose.right, pose.up, skin);

    // Compact blocky fist. No torso, legs, loincloth, head, or left arm are emitted in first person.
    addBox(mesh, arm.hand, 0.125f, skin);
    addBox(mesh, arm.hand + pose.right * 0.052f, 0.082f, skin);
    addBox(mesh, arm.hand - pose.right * 0.052f, 0.082f, skin);
    addBox(mesh, arm.hand - pose.up * 0.050f, 0.085f, skin);
    return mesh;
}

} // namespace rf::render::scene
