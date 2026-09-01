#include "world/meshing/MicroDetailBuilder.h"

#include <array>
#include <cstdint>

namespace rf::world::meshing {
namespace {

std::uint32_t detailHash(int x, int y, int z) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u;
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

float quantizedOffset(std::uint32_t value) noexcept {
    return 0.08f + static_cast<float>(value % 6u) * 0.105f;
}

bool exposedTop(const VoxelChunk& chunk, int x, int y, int z) noexcept {
    return y + 1 >= VoxelChunk::sizeY || !isSolid(chunk.get(x, y + 1, z));
}

} // namespace

void MicroDetailBuilder::append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh) {
    for (int z = 0; z < VoxelChunk::sizeZ; ++z) {
        for (int x = 0; x < VoxelChunk::sizeX; ++x) {
            for (int y = VoxelChunk::sizeY - 1; y >= 0; --y) {
                const BlockId block = snapshot.center.get(x, y, z);
                if (block == BlockId::Air || !exposedTop(snapshot.center, x, y, z)) continue;

                const std::uint32_t h = detailHash(x, y, z);
                if (block == BlockId::Grass) {
                    const float ox = quantizedOffset(h);
                    const float oz = quantizedOffset(h >> 5);
                    const float width = 0.13f + static_cast<float>((h >> 10) % 4u) * 0.025f;
                    const float height = 0.055f + static_cast<float>((h >> 14) % 5u) * 0.023f;
                    addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                           width, height, width, SurfaceMaterial::GrassTop);

                    if ((h & 3u) == 0u) {
                        const float ox2 = quantizedOffset(h >> 18);
                        const float oz2 = quantizedOffset(h >> 23);
                        addBox(mesh, static_cast<float>(x) + ox2, static_cast<float>(y + 1), static_cast<float>(z) + oz2,
                               width * 0.72f, height * 1.32f, width * 0.72f, SurfaceMaterial::GrassTop);
                    }
                } else if (block == BlockId::Stone && (h % 5u) == 0u) {
                    const float ox = 0.12f + static_cast<float>((h >> 4) % 4u) * 0.14f;
                    const float oz = 0.10f + static_cast<float>((h >> 9) % 4u) * 0.14f;
                    const float width = 0.24f + static_cast<float>((h >> 13) % 4u) * 0.055f;
                    const float depth = 0.22f + static_cast<float>((h >> 17) % 4u) * 0.05f;
                    const float height = 0.045f + static_cast<float>((h >> 21) % 4u) * 0.025f;
                    addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                           width, height, depth, SurfaceMaterial::Stone);
                } else if (block == BlockId::Leaves && (h % 3u) == 0u) {
                    const float ox = quantizedOffset(h >> 3);
                    const float oz = quantizedOffset(h >> 11);
                    const float size = 0.16f + static_cast<float>((h >> 19) % 4u) * 0.035f;
                    addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                           size, size * 0.72f, size, SurfaceMaterial::Leaves);
                }

                // Only the topmost exposed solid cell in this column can contribute top decoration.
                break;
            }
        }
    }
}

} // namespace rf::world::meshing
