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
        return MeshVertex{p[0], p[1], p[2], normal[0], normal[1], normal[2],
                          static_cast<std::uint32_t>(material)};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addBox(VoxelMesh& mesh, float x, float y, float z,
            float sx, float sy, float sz, SurfaceMaterial material) {
    const float x1 = x + sx;
    const float y1 = y + sy;
    const float z1 = z + sz;
    addQuad(mesh, {x, y, z1}, {x1, y, z1}, {x1, y1, z1}, {x, y1, z1}, {0, 0, 1}, material);
    addQuad(mesh, {x1, y, z}, {x, y, z}, {x, y1, z}, {x1, y1, z}, {0, 0, -1}, material);
    addQuad(mesh, {x1, y, z1}, {x1, y, z}, {x1, y1, z}, {x1, y1, z1}, {1, 0, 0}, material);
    addQuad(mesh, {x, y, z}, {x, y, z1}, {x, y1, z1}, {x, y1, z}, {-1, 0, 0}, material);
    addQuad(mesh, {x, y1, z1}, {x1, y1, z1}, {x1, y1, z}, {x, y1, z}, {0, 1, 0}, material);
    addQuad(mesh, {x, y, z}, {x1, y, z}, {x1, y, z1}, {x, y, z1}, {0, -1, 0}, material);
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
    constexpr float centerSize = 0.045f;
    constexpr float petalSize = 0.040f;
    addBox(mesh, cx - centerSize * 0.5f, y, cz - centerSize * 0.5f,
           centerSize, 0.032f, centerSize, material);
    addBox(mesh, cx - petalSize * 1.45f, y + 0.004f, cz - petalSize * 0.5f,
           petalSize, 0.024f, petalSize, material);
    addBox(mesh, cx + petalSize * 0.45f, y + 0.004f, cz - petalSize * 0.5f,
           petalSize, 0.024f, petalSize, material);
    addBox(mesh, cx - petalSize * 0.5f, y + 0.004f, cz - petalSize * 1.45f,
           petalSize, 0.024f, petalSize, material);
    addBox(mesh, cx - petalSize * 0.5f, y + 0.004f, cz + petalSize * 0.45f,
           petalSize, 0.024f, petalSize, material);
}

void addGrassNodes(VoxelMesh& mesh, const ChunkMeshingSnapshot& snapshot,
                   int localX, int y, int localZ,
                   const micro::MicroVoxelState* state) {
    const BlockCoord worldBlock{snapshot.worldOriginX + localX, y, snapshot.worldOriginZ + localZ};
    for (int nz = 0; nz < growth::GrassGrowth::nodeResolution; ++nz) {
        for (int nx = 0; nx < growth::GrassGrowth::nodeResolution; ++nx) {
            if (state && !state->occupied(nx, micro::resolution - 1, nz)) continue;
            const auto node = growth::GrassGrowth::sample(snapshot.worldSeed, worldBlock, nx, nz,
                                                          snapshot.worldAgeSeconds);
            if (!node.present || node.stage == 0) continue;

            const float cell = 1.0f / static_cast<float>(growth::GrassGrowth::nodeResolution);
            const float cx = static_cast<float>(localX) + (static_cast<float>(nx) + 0.5f) * cell;
            const float cz = static_cast<float>(localZ) + (static_cast<float>(nz) + 0.5f) * cell;
            const float width = node.width;
            const float stemHeight = node.height;
            addBox(mesh, cx - width * 0.5f, static_cast<float>(y + 1), cz - width * 0.5f,
                   width, stemHeight, width, SurfaceMaterial::GrassTop);
            addFlower(mesh, cx, static_cast<float>(y + 1) + stemHeight, cz, node.flower);
        }
    }
}

} // namespace

void MicroDetailBuilder::append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh) {
    // Intact coarse blocks still receive dense, deterministic micro-scale surface geometry.
    // Promoting a block for physical damage therefore does not suddenly make it "higher res".
    for (int z = 0; z < VoxelChunk::sizeZ; ++z) {
        for (int x = 0; x < VoxelChunk::sizeX; ++x) {
            for (int y = VoxelChunk::sizeY - 1; y >= 0; --y) {
                const BlockId block = snapshot.center.get(x, y, z);
                if (block == BlockId::Air || !exposedTop(snapshot.center, x, y, z)) continue;

                const int wx = snapshot.worldOriginX + x;
                const int wz = snapshot.worldOriginZ + z;
                const std::uint32_t h = detailHash(wx, y, wz, snapshot.worldSeed);
                if (block == BlockId::Grass) {
                    addGrassNodes(mesh, snapshot, x, y, z, nullptr);
                } else if (block == BlockId::Stone) {
                    // Small fractured plates make the silhouette echo the reference blocks while
                    // the shader supplies much denser seams across the entire visible surface.
                    for (int plate = 0; plate < 2; ++plate) {
                        const std::uint32_t ph = h ^ (0x9e3779b9u * static_cast<std::uint32_t>(plate + 1));
                        if ((ph % 4u) == 0u) continue;
                        const float ox = 0.06f + static_cast<float>((ph >> 4) % 6u) * 0.135f;
                        const float oz = 0.06f + static_cast<float>((ph >> 9) % 6u) * 0.135f;
                        const float width = 0.12f + static_cast<float>((ph >> 13) % 4u) * 0.045f;
                        const float depth = 0.11f + static_cast<float>((ph >> 17) % 4u) * 0.042f;
                        const float height = 0.025f + static_cast<float>((ph >> 21) % 4u) * 0.018f;
                        addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                               width, height, depth, SurfaceMaterial::Stone);
                    }
                } else if (block == BlockId::Leaves && (h % 3u) != 0u) {
                    const float ox = 0.08f + static_cast<float>((h >> 3) % 6u) * 0.13f;
                    const float oz = 0.08f + static_cast<float>((h >> 11) % 6u) * 0.13f;
                    const float size = 0.10f + static_cast<float>((h >> 19) % 4u) * 0.035f;
                    addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                           size, size * 0.72f, size, SurfaceMaterial::Leaves);
                }
                break;
            }
        }
    }

    // Promoted/halo blocks reuse the exact same growth field. Chipped-away top microcells simply
    // stop supporting their associated grass node, so the visual and physical representations agree.
    for (const auto& block : snapshot.microBlocks) {
        if (!block.owned || block.block != BlockId::Grass) continue;
        addGrassNodes(mesh, snapshot, block.localX, block.y, block.localZ, &block.state);
    }
}

} // namespace rf::world::meshing
