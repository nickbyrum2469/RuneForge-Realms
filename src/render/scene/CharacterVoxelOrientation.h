#pragma once

#include "game/character/PlayerBodyRig.h"
#include "world/GreedyMesher.h"

#include <cstddef>

namespace rf::render::scene {

// VoxelCharacterBuilder currently emits each micro-cube around an already character-local center,
// but the cube's faces/normals are authored on the world XYZ axes. That makes the hero look correct
// only when facing a world axis and turns every body pixel into a diamond at intermediate yaw angles.
//
// Reorient each emitted box around its own center into the actor's right/up/forward basis. Positions
// along the articulated rig are left untouched; only the local cube basis changes. The builder emits
// one box as 6 quads * 4 vertices = 24 consecutive vertices, so this operation is deterministic and
// does not alter indices, materials, anatomy, limb lengths, or terrain/world orientation.
inline void orientCharacterVoxels(world::VoxelMesh& mesh,
                                  const game::character::PlayerBodyPose& pose) noexcept {
    constexpr std::size_t verticesPerBox = 24;
    if (mesh.vertices.size() % verticesPerBox != 0) return;

    for (std::size_t base = 0; base < mesh.vertices.size(); base += verticesPerBox) {
        game::Vec3 center{};
        for (std::size_t i = 0; i < verticesPerBox; ++i) {
            const auto& vertex = mesh.vertices[base + i];
            center = center + game::Vec3{vertex.x, vertex.y, vertex.z};
        }
        center = center * (1.0f / static_cast<float>(verticesPerBox));

        for (std::size_t i = 0; i < verticesPerBox; ++i) {
            auto& vertex = mesh.vertices[base + i];
            const game::Vec3 offset{vertex.x - center.x, vertex.y - center.y, vertex.z - center.z};
            const game::Vec3 rotated = center + pose.right * offset.x + pose.up * offset.y +
                                       pose.forward * offset.z;
            vertex.x = rotated.x;
            vertex.y = rotated.y;
            vertex.z = rotated.z;

            const game::Vec3 normal = pose.right * vertex.nx + pose.up * vertex.ny +
                                      pose.forward * vertex.nz;
            vertex.nx = normal.x;
            vertex.ny = normal.y;
            vertex.nz = normal.z;
        }
    }
}

} // namespace rf::render::scene
