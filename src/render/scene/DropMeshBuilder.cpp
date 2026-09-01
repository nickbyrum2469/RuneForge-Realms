#include "render/scene/DropMeshBuilder.h"

#include "game/items/ItemId.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace rf::render::scene {
namespace {

world::SurfaceMaterial materialFor(game::items::ItemId item) noexcept {
    switch (item) {
        case game::items::ItemId::GrassBlock: return world::SurfaceMaterial::GrassTop;
        case game::items::ItemId::DirtBlock: return world::SurfaceMaterial::Dirt;
        case game::items::ItemId::StoneBlock: return world::SurfaceMaterial::Stone;
        case game::items::ItemId::OakLog: return world::SurfaceMaterial::WoodBark;
        case game::items::ItemId::Leaves: return world::SurfaceMaterial::Leaves;
        case game::items::ItemId::None: break;
    }
    return world::SurfaceMaterial::Stone;
}

void emitQuad(world::VoxelMesh& mesh,
              std::array<float, 3> p0, std::array<float, 3> p1,
              std::array<float, 3> p2, std::array<float, 3> p3,
              std::array<float, 3> normal, world::SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto vertex = [&](const std::array<float, 3>& p) {
        return world::MeshVertex{p[0], p[1], p[2], normal[0], normal[1], normal[2],
                                 static_cast<std::uint32_t>(material)};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addBox(world::VoxelMesh& mesh, float x, float y, float z, float size,
            world::SurfaceMaterial material) {
    const float x1 = x + size;
    const float y1 = y + size;
    const float z1 = z + size;
    emitQuad(mesh, {x, y, z1}, {x1, y, z1}, {x1, y1, z1}, {x, y1, z1}, {0, 0, 1}, material);
    emitQuad(mesh, {x1, y, z}, {x, y, z}, {x, y1, z}, {x1, y1, z}, {0, 0, -1}, material);
    emitQuad(mesh, {x1, y, z1}, {x1, y, z}, {x1, y1, z}, {x1, y1, z1}, {1, 0, 0}, material);
    emitQuad(mesh, {x, y, z}, {x, y, z1}, {x, y1, z1}, {x, y1, z}, {-1, 0, 0}, material);
    emitQuad(mesh, {x, y1, z1}, {x1, y1, z1}, {x1, y1, z}, {x, y1, z}, {0, 1, 0}, material);
    emitQuad(mesh, {x, y, z}, {x1, y, z}, {x1, y, z1}, {x, y, z1}, {0, -1, 0}, material);
}

} // namespace

world::VoxelMesh DropMeshBuilder::build(const std::vector<game::drops::WorldDrop>& drops) {
    world::VoxelMesh mesh;
    constexpr float size = 0.28f;
    for (const auto& drop : drops) {
        const float bob = std::sin(drop.age * 2.6f + static_cast<float>(drop.id % 97u) * 0.31f) * 0.035f;
        addBox(mesh,
               drop.position.x - size * 0.5f,
               drop.position.y + bob,
               drop.position.z - size * 0.5f,
               size,
               materialFor(drop.item));
    }
    return mesh;
}

} // namespace rf::render::scene
