#pragma once

#include "game/character/PlayerBodyRig.h"
#include "world/GreedyMesher.h"

#include <cstddef>

namespace rf::render::scene {

// Character geometry is authored around character-local centers, but VoxelCharacterBuilder emits
// each tiny box on world XYZ axes. Rotate every emitted box around its own center into the actor's
// right/up/forward basis so the hero preserves the same clean voxel silhouette at every yaw.
// Terrain/world blocks are deliberately untouched.
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
