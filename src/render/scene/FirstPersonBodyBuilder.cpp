#include "render/scene/FirstPersonBodyBuilder.h"

#include <array>
#include <cmath>

namespace rf::render::scene {
namespace {

void addQuad(world::VoxelMesh& mesh,
             const std::array<float,3>& p0, const std::array<float,3>& p1,
             const std::array<float,3>& p2, const std::array<float,3>& p3,
             const std::array<float,3>& normal, world::SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const std::uint32_t packed = world::packMaterial(material);
    auto v = [&](const std::array<float,3>& p) {
        return world::MeshVertex{p[0],p[1],p[2],normal[0],normal[1],normal[2],packed};
    };
    mesh.vertices.insert(mesh.vertices.end(), {v(p0),v(p1),v(p2),v(p3)});
    mesh.indices.insert(mesh.indices.end(), {base,base+1,base+2,base,base+2,base+3});
    ++mesh.quadCount;
}

void addBox(world::VoxelMesh& mesh, game::Vec3 center, float sx, float sy, float sz,
            world::SurfaceMaterial material) {
    const float x0 = center.x - sx*0.5f, x1 = center.x + sx*0.5f;
    const float y0 = center.y - sy*0.5f, y1 = center.y + sy*0.5f;
    const float z0 = center.z - sz*0.5f, z1 = center.z + sz*0.5f;
    addQuad(mesh,{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1},{0,0,1},material);
    addQuad(mesh,{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0},{0,0,-1},material);
    addQuad(mesh,{x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1},{1,0,0},material);
    addQuad(mesh,{x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0},{-1,0,0},material);
    addQuad(mesh,{x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0},{0,1,0},material);
    addQuad(mesh,{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},{0,-1,0},material);
}

void addVoxelChain(world::VoxelMesh& mesh, game::Vec3 from, game::Vec3 to, int count,
                   float size, world::SurfaceMaterial material) {
    for (int i = 0; i < count; ++i) {
        const float t = count <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(count - 1);
        const game::Vec3 p = from * (1.0f - t) + to * t;
        addBox(mesh, p, size, size, size, material);
    }
}

void addHand(world::VoxelMesh& mesh, game::Vec3 hand, game::Vec3 right, game::Vec3 up,
             bool armored) {
    addBox(mesh, hand, 0.16f, 0.15f, 0.15f, world::SurfaceMaterial::CharacterSkin);
    addBox(mesh, hand - up * 0.105f, 0.17f, 0.08f, 0.17f, world::SurfaceMaterial::CharacterLeather);
    if (armored) {
        addBox(mesh, hand + right * 0.055f + up * 0.018f, 0.055f, 0.055f, 0.16f,
               world::SurfaceMaterial::CharacterMetal);
    }
}

} // namespace

world::VoxelMesh FirstPersonBodyBuilder::build(game::Vec3 eye,
                                               game::Vec3 forward,
                                               game::Vec3 right,
                                               game::Vec3 up,
                                               const game::interaction::SwingPose& swingPose) {
    world::VoxelMesh mesh;

    game::Vec3 rightShoulder = eye + right * 0.31f - up * 0.30f - forward * 0.02f;
    game::Vec3 rightElbow = eye + forward * 0.23f + right * 0.34f - up * 0.43f;
    game::Vec3 rightHand = eye + forward * 0.46f + right * 0.30f - up * 0.36f;
    if (swingPose.active) {
        rightShoulder = swingPose.shoulder;
        rightElbow = swingPose.elbow;
        rightHand = swingPose.hand;
    }

    addVoxelChain(mesh, rightShoulder, rightElbow, 5, 0.115f, world::SurfaceMaterial::CharacterBlueCloth);
    addVoxelChain(mesh, rightElbow, rightHand - up * 0.07f, 5, 0.105f, world::SurfaceMaterial::CharacterLeather);
    addHand(mesh, rightHand, right, up, true);

    const game::Vec3 leftShoulder = eye - right * 0.31f - up * 0.30f - forward * 0.02f;
    const game::Vec3 leftElbow = eye + forward * 0.18f - right * 0.34f - up * 0.46f;
    const game::Vec3 leftHand = eye + forward * 0.40f - right * 0.31f - up * 0.41f;
    addVoxelChain(mesh, leftShoulder, leftElbow, 5, 0.112f, world::SurfaceMaterial::CharacterBlueCloth);
    addVoxelChain(mesh, leftElbow, leftHand - up * 0.06f, 4, 0.102f, world::SurfaceMaterial::CharacterLeather);
    addHand(mesh, leftHand, right * -1.0f, up, false);

    return mesh;
}

} // namespace rf::render::scene
