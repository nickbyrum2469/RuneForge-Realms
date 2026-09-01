#include "world/GreedyMesher.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rf::world {
namespace {

struct FaceKey {
    bool visible{false};
    int sign{};
    SurfaceMaterial material{SurfaceMaterial::Dirt};
    std::uint8_t damageStage{};

    [[nodiscard]] bool operator==(const FaceKey& other) const noexcept {
        return visible == other.visible && sign == other.sign && material == other.material &&
               damageStage == other.damageStage;
    }
};

constexpr std::array<int, 3> dimensions{VoxelChunk::sizeX, VoxelChunk::sizeY, VoxelChunk::sizeZ};

BlockId sample(const ChunkMeshingSnapshot& snapshot, int x, int y, int z) noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;

    if (x < 0) return snapshot.negativeX ? snapshot.negativeX->get(VoxelChunk::sizeX - 1, y, z) : BlockId::Air;
    if (x >= VoxelChunk::sizeX) return snapshot.positiveX ? snapshot.positiveX->get(0, y, z) : BlockId::Air;
    if (z < 0) return snapshot.negativeZ ? snapshot.negativeZ->get(x, y, VoxelChunk::sizeZ - 1) : BlockId::Air;
    if (z >= VoxelChunk::sizeZ) return snapshot.positiveZ ? snapshot.positiveZ->get(x, y, 0) : BlockId::Air;
    return snapshot.center.get(x, y, z);
}

std::uint8_t damageStageAt(const ChunkMeshingSnapshot& snapshot, int x, int y, int z) noexcept {
    const BlockCoord worldPosition{snapshot.worldOriginX + x, y, snapshot.worldOriginZ + z};
    for (const auto& state : snapshot.damageBlocks) {
        if (state.position == worldPosition) return state.stage;
    }
    return 0;
}

void emitQuad(VoxelMesh& mesh, const std::array<int, 3>& origin,
              const std::array<int, 3>& du, const std::array<int, 3>& dv,
              int axis, int sign, SurfaceMaterial material, std::uint8_t damageStage) {
    const float normal[3]{
        axis == 0 ? static_cast<float>(sign) : 0.0f,
        axis == 1 ? static_cast<float>(sign) : 0.0f,
        axis == 2 ? static_cast<float>(sign) : 0.0f,
    };
    const std::uint32_t packed = packMaterial(material, damageStage);

    auto vertex = [&](const std::array<int, 3>& p) {
        return MeshVertex{static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]),
                          normal[0], normal[1], normal[2], packed};
    };

    const std::array<int, 3> p0 = origin;
    const std::array<int, 3> p1{origin[0] + du[0], origin[1] + du[1], origin[2] + du[2]};
    const std::array<int, 3> p2{p1[0] + dv[0], p1[1] + dv[1], p1[2] + dv[2]};
    const std::array<int, 3> p3{origin[0] + dv[0], origin[1] + dv[1], origin[2] + dv[2]};

    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    if (sign > 0) mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    else mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p3), vertex(p2), vertex(p1)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

} // namespace

void VoxelMesh::append(const VoxelMesh& source, float offsetX, float offsetY, float offsetZ) {
    const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
    vertices.reserve(vertices.size() + source.vertices.size());
    indices.reserve(indices.size() + source.indices.size());
    for (auto vertex : source.vertices) {
        vertex.x += offsetX;
        vertex.y += offsetY;
        vertex.z += offsetZ;
        vertices.push_back(vertex);
    }
    for (std::uint32_t index : source.indices) indices.push_back(base + index);
    quadCount += source.quadCount;
}

VoxelMesh GreedyMesher::build(const VoxelChunk& chunk) {
    ChunkMeshingSnapshot snapshot;
    snapshot.center = chunk;
    return build(snapshot);
}

VoxelMesh GreedyMesher::build(const ChunkMeshingSnapshot& snapshot) {
    VoxelMesh mesh;
    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        const int width = dimensions[u];
        const int height = dimensions[v];
        std::vector<FaceKey> mask(static_cast<std::size_t>(width * height));

        for (int slice = -1; slice < dimensions[axis]; ++slice) {
            for (int j = 0; j < height; ++j) {
                for (int i = 0; i < width; ++i) {
                    std::array<int, 3> a{};
                    std::array<int, 3> b{};
                    a[axis] = slice;
                    b[axis] = slice + 1;
                    a[u] = b[u] = i;
                    a[v] = b[v] = j;
                    const BlockId blockA = sample(snapshot, a[0], a[1], a[2]);
                    const BlockId blockB = sample(snapshot, b[0], b[1], b[2]);
                    FaceKey face{};
                    if (isSolid(blockA) && !isSolid(blockB)) {
                        face = {true, +1, surfaceMaterial(blockA, axis, +1),
                                damageStageAt(snapshot, a[0], a[1], a[2])};
                    } else if (!isSolid(blockA) && isSolid(blockB)) {
                        face = {true, -1, surfaceMaterial(blockB, axis, -1),
                                damageStageAt(snapshot, b[0], b[1], b[2])};
                    }
                    mask[static_cast<std::size_t>(i + width * j)] = face;
                }
            }

            for (int j = 0; j < height; ++j) {
                for (int i = 0; i < width;) {
                    const FaceKey key = mask[static_cast<std::size_t>(i + width * j)];
                    if (!key.visible) { ++i; continue; }

                    int rectWidth = 1;
                    while (i + rectWidth < width &&
                           mask[static_cast<std::size_t>(i + rectWidth + width * j)] == key) ++rectWidth;

                    int rectHeight = 1;
                    bool grow = true;
                    while (j + rectHeight < height && grow) {
                        for (int x = 0; x < rectWidth; ++x) {
                            if (!(mask[static_cast<std::size_t>(i + x + width * (j + rectHeight))] == key)) {
                                grow = false;
                                break;
                            }
                        }
                        if (grow) ++rectHeight;
                    }

                    std::array<int, 3> origin{};
                    origin[axis] = slice + 1;
                    origin[u] = i;
                    origin[v] = j;
                    std::array<int, 3> du{};
                    std::array<int, 3> dv{};
                    du[u] = rectWidth;
                    dv[v] = rectHeight;
                    emitQuad(mesh, origin, du, dv, axis, key.sign, key.material, key.damageStage);

                    for (int y = 0; y < rectHeight; ++y) {
                        for (int x = 0; x < rectWidth; ++x) {
                            mask[static_cast<std::size_t>(i + x + width * (j + y))] = {};
                        }
                    }
                    i += rectWidth;
                }
            }
        }
    }
    return mesh;
}

} // namespace rf::world
