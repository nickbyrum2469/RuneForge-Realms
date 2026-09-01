#include "world/meshing/MicroVoxelMesher.h"

#include "world/micro/MicroVoxelState.h"

#include <array>
#include <cstdint>

namespace rf::world::meshing {
namespace {

BlockId sampleBlock(const ChunkMeshingSnapshot& snapshot, int x, int y, int z) noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    if (x < 0) {
        return snapshot.negativeX ? snapshot.negativeX->get(VoxelChunk::sizeX - 1, y, z) : BlockId::Air;
    }
    if (x >= VoxelChunk::sizeX) {
        return snapshot.positiveX ? snapshot.positiveX->get(0, y, z) : BlockId::Air;
    }
    if (z < 0) {
        return snapshot.negativeZ ? snapshot.negativeZ->get(x, y, VoxelChunk::sizeZ - 1) : BlockId::Air;
    }
    if (z >= VoxelChunk::sizeZ) {
        return snapshot.positiveZ ? snapshot.positiveZ->get(x, y, 0) : BlockId::Air;
    }
    return snapshot.center.get(x, y, z);
}

const micro::MicroBlockSnapshot* findMicro(const ChunkMeshingSnapshot& snapshot,
                                            int x, int y, int z) noexcept {
    if (x < 0 || x >= VoxelChunk::sizeX || z < 0 || z >= VoxelChunk::sizeZ) return nullptr;
    for (const auto& block : snapshot.microBlocks) {
        if (block.localX == x && block.y == y && block.localZ == z) return &block;
    }
    return nullptr;
}

bool occupiedAt(const ChunkMeshingSnapshot& snapshot,
                int blockX, int blockY, int blockZ,
                int microX, int microY, int microZ) noexcept {
    while (microX < 0) { --blockX; microX += micro::resolution; }
    while (microX >= micro::resolution) { ++blockX; microX -= micro::resolution; }
    while (microY < 0) { --blockY; microY += micro::resolution; }
    while (microY >= micro::resolution) { ++blockY; microY -= micro::resolution; }
    while (microZ < 0) { --blockZ; microZ += micro::resolution; }
    while (microZ >= micro::resolution) { ++blockZ; microZ -= micro::resolution; }

    if (blockY < 0 || blockY >= VoxelChunk::sizeY) return false;
    if (const auto* promoted = findMicro(snapshot, blockX, blockY, blockZ)) {
        return promoted->state.occupied(microX, microY, microZ);
    }
    return isSolid(sampleBlock(snapshot, blockX, blockY, blockZ));
}

SurfaceMaterial microMaterial(BlockId block, int microY, int axis, int sign) noexcept {
    if (block == BlockId::Grass) {
        if (axis == 1 && sign > 0 && microY == micro::resolution - 1) return SurfaceMaterial::GrassTop;
        return SurfaceMaterial::GrassSide;
    }
    return surfaceMaterial(block, axis, sign);
}

void emitFace(VoxelMesh& mesh,
              float x0, float y0, float z0,
              float x1, float y1, float z1,
              int axis, int sign, SurfaceMaterial material) {
    std::array<std::array<float, 3>, 4> p{};
    std::array<float, 3> n{};
    n[static_cast<std::size_t>(axis)] = static_cast<float>(sign);

    if (axis == 0) {
        const float x = sign > 0 ? x1 : x0;
        p = {{{x, y0, z0}, {x, y1, z0}, {x, y1, z1}, {x, y0, z1}}};
    } else if (axis == 1) {
        const float y = sign > 0 ? y1 : y0;
        p = {{{x0, y, z0}, {x0, y, z1}, {x1, y, z1}, {x1, y, z0}}};
    } else {
        const float z = sign > 0 ? z1 : z0;
        p = {{{x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}}};
    }

    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    auto vertex = [&](const std::array<float, 3>& value) {
        return MeshVertex{value[0], value[1], value[2], n[0], n[1], n[2],
                          static_cast<std::uint32_t>(material)};
    };

    if (sign > 0) {
        mesh.vertices.insert(mesh.vertices.end(), {vertex(p[0]), vertex(p[1]), vertex(p[2]), vertex(p[3])});
    } else {
        mesh.vertices.insert(mesh.vertices.end(), {vertex(p[0]), vertex(p[3]), vertex(p[2]), vertex(p[1])});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

} // namespace

void MicroVoxelMesher::append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh) {
    constexpr std::array<std::array<int, 3>, 6> directions{{
        {{-1, 0, 0}}, {{1, 0, 0}}, {{0, -1, 0}}, {{0, 1, 0}}, {{0, 0, -1}}, {{0, 0, 1}}
    }};

    for (const auto& block : snapshot.microBlocks) {
        for (int my = 0; my < micro::resolution; ++my) {
            for (int mz = 0; mz < micro::resolution; ++mz) {
                for (int mx = 0; mx < micro::resolution; ++mx) {
                    if (!block.state.occupied(mx, my, mz)) continue;

                    const float x0 = static_cast<float>(block.localX) + static_cast<float>(mx) * micro::cellSize;
                    const float y0 = static_cast<float>(block.y) + static_cast<float>(my) * micro::cellSize;
                    const float z0 = static_cast<float>(block.localZ) + static_cast<float>(mz) * micro::cellSize;
                    const float x1 = x0 + micro::cellSize;
                    const float y1 = y0 + micro::cellSize;
                    const float z1 = z0 + micro::cellSize;

                    for (const auto& direction : directions) {
                        const int nx = mx + direction[0];
                        const int ny = my + direction[1];
                        const int nz = mz + direction[2];
                        if (occupiedAt(snapshot, block.localX, block.y, block.localZ, nx, ny, nz)) continue;

                        const int axis = direction[0] != 0 ? 0 : (direction[1] != 0 ? 1 : 2);
                        const int sign = direction[static_cast<std::size_t>(axis)] > 0 ? 1 : -1;
                        emitFace(mesh, x0, y0, z0, x1, y1, z1, axis, sign,
                                 microMaterial(block.block, my, axis, sign));
                    }
                }
            }
        }
    }
}

} // namespace rf::world::meshing
