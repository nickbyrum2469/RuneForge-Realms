#include "world/meshing/MicroDetailBuilder.h"

#include "world/growth/GrassGrowth.h"

#include <array>
#include <cstdint>

namespace rf::world::meshing {
namespace {

std::uint32_t detailHash(int x, int y, int z, std::uint32_t seed) noexcept {
    std::uint32_t h = seed ^ static_cast<std::uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
    h ^= static_cast<std::uint32_t>(z) * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    return h;
}

void addQuad(VoxelMesh& mesh,
             std::array<float, 3> p0, std::array<float, 3> p1,
             std::array<float, 3> p2, std::array<float, 3> p3,
             std::array<float, 3> normal, SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto vertex = [&](const std::array<float, 3>& p) {
        return MeshVertex{p[0], p[1], p[2], normal[0], normal[1], normal[2], packMaterial(material)};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addBox(VoxelMesh& mesh, float x, float y, float z,
            float sx, float sy, float sz, SurfaceMaterial material) {
    const float x1 = x + sx, y1 = y + sy, z1 = z + sz;
    addQuad(mesh, {x,y,z1}, {x1,y,z1}, {x1,y1,z1}, {x,y1,z1}, {0,0,1}, material);
    addQuad(mesh, {x1,y,z}, {x,y,z}, {x,y1,z}, {x1,y1,z}, {0,0,-1}, material);
    addQuad(mesh, {x1,y,z1}, {x1,y,z}, {x1,y1,z}, {x1,y1,z1}, {1,0,0}, material);
    addQuad(mesh, {x,y,z}, {x,y,z1}, {x,y1,z1}, {x,y1,z}, {-1,0,0}, material);
    addQuad(mesh, {x,y1,z1}, {x1,y1,z1}, {x1,y1,z}, {x,y1,z}, {0,1,0}, material);
    addQuad(mesh, {x,y,z}, {x1,y,z}, {x1,y,z1}, {x,y,z1}, {0,-1,0}, material);
}

bool exposedTop(const VoxelChunk& chunk, int x, int y, int z) noexcept {
    return y + 1 >= VoxelChunk::sizeY || !isSolid(chunk.get(x, y + 1, z));
}

SurfaceMaterial flowerMaterial(growth::FlowerType type) noexcept {
    switch (type) {
        case growth::FlowerType::White: return SurfaceMaterial::FlowerWhite;
        case growth::FlowerType::Yellow: return SurfaceMaterial::FlowerYellow;
        case growth::FlowerType::Blue: return SurfaceMaterial::FlowerBlue;
        case growth::FlowerType::None: break;
    }
    return SurfaceMaterial::GrassTop;
}

void addFlower(VoxelMesh& mesh, float cx, float y, float cz, growth::FlowerType type) {
    if (type == growth::FlowerType::None) return;
    const auto material = flowerMaterial(type);
    constexpr float center = 0.022f;
    constexpr float petal = 0.020f;
    addBox(mesh, cx - center * 0.5f, y, cz - center * 0.5f, center, 0.018f, center, material);
    addBox(mesh, cx - petal * 1.30f, y + 0.002f, cz - petal * 0.5f, petal, 0.014f, petal, material);
    addBox(mesh, cx + petal * 0.30f, y + 0.002f, cz - petal * 0.5f, petal, 0.014f, petal, material);
    addBox(mesh, cx - petal * 0.5f, y + 0.002f, cz - petal * 1.30f, petal, 0.014f, petal, material);
    addBox(mesh, cx - petal * 0.5f, y + 0.002f, cz + petal * 0.30f, petal, 0.014f, petal, material);
}

bool keepGrassNode(SurfaceDetailTier tier, std::uint32_t hash) noexcept {
    if (tier == SurfaceDetailTier::Distant) return false;
    if (tier == SurfaceDetailTier::Hero) return true;
    return (hash & 1u) == 0u;
}

void addGrassNodes(VoxelMesh& mesh, const ChunkMeshingSnapshot& snapshot,
                   int localX, int y, int localZ,
                   const micro::MicroVoxelState* state, SurfaceDetailTier tier) {
    if (tier == SurfaceDetailTier::Distant) return;
    const BlockCoord worldBlock{snapshot.worldOriginX + localX, y, snapshot.worldOriginZ + localZ};
    const std::uint32_t baseHash = detailHash(worldBlock.x, worldBlock.y, worldBlock.z, snapshot.worldSeed);
    for (int nz = 0; nz < growth::GrassGrowth::nodeResolution; ++nz) {
        for (int nx = 0; nx < growth::GrassGrowth::nodeResolution; ++nx) {
            if (state && !state->occupied(nx, micro::resolution - 1, nz)) continue;
            const std::uint32_t nodeHash = baseHash ^
                (static_cast<std::uint32_t>(nx + nz * growth::GrassGrowth::nodeResolution + 1) * 0x9e3779b9u);
            if (!keepGrassNode(tier, nodeHash)) continue;

            const auto node = growth::GrassGrowth::sample(snapshot.worldSeed, worldBlock, nx, nz,
                                                          snapshot.worldAgeSeconds);
            if (!node.present || node.stage == 0) continue;

            const float cell = 1.0f / static_cast<float>(growth::GrassGrowth::nodeResolution);
            const float jitterX = (static_cast<float>((nodeHash >> 7) & 15u) / 15.0f - 0.5f) * cell * 0.34f;
            const float jitterZ = (static_cast<float>((nodeHash >> 13) & 15u) / 15.0f - 0.5f) * cell * 0.34f;
            const float cx = static_cast<float>(localX) + (static_cast<float>(nx) + 0.5f) * cell + jitterX;
            const float cz = static_cast<float>(localZ) + (static_cast<float>(nz) + 0.5f) * cell + jitterZ;
            const float baseY = static_cast<float>(y + 1);
            const float w = node.width;
            const float h = node.height;

            // A growth node is a tiny tuft rather than one fat pillar. Different blade heights keep the
            // reference silhouette broken and soft while remaining real voxel geometry.
            addBox(mesh, cx - w * 0.45f, baseY, cz - w * 0.45f, w * 0.65f, h, w * 0.65f, SurfaceMaterial::GrassTop);
            if (node.stage >= 2) {
                addBox(mesh, cx + w * 0.12f, baseY, cz - w * 0.18f, w * 0.52f, h * 0.78f, w * 0.52f, SurfaceMaterial::GrassTop);
            }
            if (node.stage >= 3 && tier == SurfaceDetailTier::Hero) {
                addBox(mesh, cx - w * 0.62f, baseY, cz + w * 0.10f, w * 0.44f, h * 0.62f, w * 0.44f, SurfaceMaterial::GrassTop);
            }
            addFlower(mesh, cx, baseY + h, cz, node.flower);
        }
    }
}

} // namespace

void MicroDetailBuilder::append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                                SurfaceDetailTier tier) {
    if (tier != SurfaceDetailTier::Distant) {
        for (int z = 0; z < VoxelChunk::sizeZ; ++z) {
            for (int x = 0; x < VoxelChunk::sizeX; ++x) {
                for (int y = VoxelChunk::sizeY - 1; y >= 0; --y) {
                    const BlockId block = snapshot.center.get(x, y, z);
                    if (block == BlockId::Air || !exposedTop(snapshot.center, x, y, z)) continue;

                    const int wx = snapshot.worldOriginX + x;
                    const int wz = snapshot.worldOriginZ + z;
                    const std::uint32_t h = detailHash(wx, y, wz, snapshot.worldSeed);
                    if (block == BlockId::Grass) {
                        addGrassNodes(mesh, snapshot, x, y, z, nullptr, tier);
                    } else if (block == BlockId::Stone) {
                        const int plateCount = tier == SurfaceDetailTier::Hero ? 4 : 2;
                        for (int plate = 0; plate < plateCount; ++plate) {
                            const std::uint32_t ph = h ^ (0x9e3779b9u * static_cast<std::uint32_t>(plate + 1));
                            if ((ph % 5u) == 0u) continue;
                            const float ox = 0.05f + static_cast<float>((ph >> 4) % 8u) * 0.108f;
                            const float oz = 0.05f + static_cast<float>((ph >> 9) % 8u) * 0.108f;
                            const float width = 0.075f + static_cast<float>((ph >> 13) % 5u) * 0.031f;
                            const float depth = 0.070f + static_cast<float>((ph >> 17) % 5u) * 0.029f;
                            const float height = 0.014f + static_cast<float>((ph >> 21) % 4u) * 0.010f;
                            addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                                   width, height, depth, SurfaceMaterial::Stone);
                        }
                    } else if (block == BlockId::Leaves) {
                        const int clusterCount = tier == SurfaceDetailTier::Hero ? 2 : 1;
                        for (int cluster = 0; cluster < clusterCount; ++cluster) {
                            const std::uint32_t lh = h ^ (0x632be59bu * static_cast<std::uint32_t>(cluster + 1));
                            if ((lh % 4u) == 0u) continue;
                            const float ox = 0.07f + static_cast<float>((lh >> 3) % 7u) * 0.125f;
                            const float oz = 0.07f + static_cast<float>((lh >> 11) % 7u) * 0.125f;
                            const float size = 0.065f + static_cast<float>((lh >> 19) % 4u) * 0.025f;
                            addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                                   size, size * 0.58f, size, SurfaceMaterial::Leaves);
                        }
                    }
                    break;
                }
            }
        }
    }

    for (const auto& block : snapshot.microBlocks) {
        if (!block.owned || block.block != BlockId::Grass) continue;
        addGrassNodes(mesh, snapshot, block.localX, block.y, block.localZ, &block.state, tier);
    }
}

} // namespace rf::world::meshing
