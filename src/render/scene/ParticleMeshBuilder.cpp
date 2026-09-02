#include "render/scene/ParticleMeshBuilder.h"

#include <array>
#include <cstdint>

namespace rf::render::scene {
namespace {

world::SurfaceMaterial materialFor(world::BlockId block) noexcept {
    switch (block) {
        case world::BlockId::Grass: return world::SurfaceMaterial::GrassTop;
        case world::BlockId::Dirt: return world::SurfaceMaterial::Dirt;
        case world::BlockId::Stone: return world::SurfaceMaterial::Stone;
        case world::BlockId::Wood: return world::SurfaceMaterial::WoodBark;
        case world::BlockId::Leaves: return world::SurfaceMaterial::Leaves;
        case world::BlockId::Water: return world::SurfaceMaterial::Water;
        case world::BlockId::Air: break;
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
                                 world::packMaterial(material)};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addBox(world::VoxelMesh& mesh, float x, float y, float z, float size,
            world::SurfaceMaterial material) {
    const float x1 = x + size, y1 = y + size, z1 = z + size;
    emitQuad(mesh, {x,y,z1}, {x1,y,z1}, {x1,y1,z1}, {x,y1,z1}, {0,0,1}, material);
    emitQuad(mesh, {x1,y,z}, {x,y,z}, {x,y1,z}, {x1,y1,z}, {0,0,-1}, material);
    emitQuad(mesh, {x1,y,z1}, {x1,y,z}, {x1,y1,z}, {x1,y1,z1}, {1,0,0}, material);
    emitQuad(mesh, {x,y,z}, {x,y,z1}, {x,y1,z1}, {x,y1,z}, {-1,0,0}, material);
    emitQuad(mesh, {x,y1,z1}, {x1,y1,z1}, {x1,y1,z}, {x,y1,z}, {0,1,0}, material);
    emitQuad(mesh, {x,y,z}, {x1,y,z}, {x1,y,z1}, {x,y,z1}, {0,-1,0}, material);
}

} // namespace

world::VoxelMesh ParticleMeshBuilder::build(const std::vector<game::particles::BlockParticle>& particles) {
    world::VoxelMesh mesh;
    for (const auto& particle : particles) {
        const float fade = 1.0f - particle.age / particle.lifetime;
        const float size = particle.size * (0.72f + fade * 0.28f);
        addBox(mesh,
               particle.position.x - size * 0.5f,
               particle.position.y - size * 0.5f,
               particle.position.z - size * 0.5f,
               size,
               materialFor(particle.block));
    }
    return mesh;
}

} // namespace rf::render::scene
