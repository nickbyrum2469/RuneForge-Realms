#include "world/GreedyMesher.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rf::world {
namespace {

struct FaceKey {
    bool visible{false};
    int sign{0};
    SurfaceMaterial material{SurfaceMaterial::Dirt};

    [[nodiscard]] bool operator==(const FaceKey& other) const noexcept {
        return visible == other.visible && sign == other.sign && material == other.material;
    }
};

constexpr std::array<int, 3> kDimensions{VoxelChunk::sizeX, VoxelChunk::sizeY, VoxelChunk::sizeZ};

BlockId sample(const VoxelChunk& chunk, const std::array<int, 3>& p) {
    return chunk.get(p[0], p[1], p[2]);
}

void emitQuad(ChunkMesh& mesh,
              const std::array<int, 3>& origin,
              const std::array<int, 3>& du,
              const std::array<int, 3>& dv,
              int axis,
              int sign,
              SurfaceMaterial material) {
    const float normal[3] = {
        axis == 0 ? static_cast<float>(sign) : 0.0f,
        axis == 1 ? static_cast<float>(sign) : 0.0f,
        axis == 2 ? static_cast<float>(sign) : 0.0f,
    };

    auto makeVertex = [&](const std::array<int, 3>& p) {
        return MeshVertex{
            static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]),
            normal[0], normal[1], normal[2], static_cast<std::uint32_t>(material),
        };
    };

    const std::array<int, 3> p0 = origin;
    const std::array<int, 3> p1{origin[0] + du[0], origin[1] + du[1], origin[2] + du[2]};
    const std::array<int, 3> p2{p1[0] + dv[0], p1[1] + dv[1], p1[2] + dv[2]};
    const std::array<int, 3> p3{origin[0] + dv[0], origin[1] + dv[1], origin[2] + dv[2]};

    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    if (sign > 0) {
        mesh.vertices.push_back(makeVertex(p0));
        mesh.vertices.push_back(makeVertex(p1));
        mesh.vertices.push_back(makeVertex(p2));
        mesh.vertices.push_back(makeVertex(p3));
    } else {
        // Reverse the winding while retaining the same geometric rectangle.
        mesh.vertices.push_back(makeVertex(p0));
        mesh.vertices.push_back(makeVertex(p3));
        mesh.vertices.push_back(makeVertex(p2));
        mesh.vertices.push_back(makeVertex(p1));
    }

    mesh.indices.insert(mesh.indices.end(), {
        base + 0, base + 1, base + 2,
        base + 0, base + 2, base + 3,
    });
    ++mesh.quadCount;
}

} // namespace

ChunkMesh GreedyMesher::build(const VoxelChunk& chunk) {
    ChunkMesh mesh;

    // Each axis is swept one boundary plane at a time. For a plane, the mask stores only
    // visible transitions from solid->air or air->solid. Equal material + normal cells are
    // merged into maximal rectangles, turning long voxel surfaces into one quad.
    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        const int width = kDimensions[u];
        const int height = kDimensions[v];
        std::vector<FaceKey> mask(static_cast<std::size_t>(width * height));

        for (int slice = -1; slice < kDimensions[axis]; ++slice) {
            for (int j = 0; j < height; ++j) {
                for (int i = 0; i < width; ++i) {
                    std::array<int, 3> a{};
                    std::array<int, 3> b{};
                    a[axis] = slice;
                    b[axis] = slice + 1;
                    a[u] = b[u] = i;
                    a[v] = b[v] = j;

                    const BlockId blockA = sample(chunk, a);
                    const BlockId blockB = sample(chunk, b);
                    FaceKey face{};

                    if (isSolid(blockA) && !isSolid(blockB)) {
                        face.visible = true;
                        face.sign = +1;
                        face.material = surfaceMaterial(blockA, axis, +1);
                    } else if (!isSolid(blockA) && isSolid(blockB)) {
                        face.visible = true;
                        face.sign = -1;
                        face.material = surfaceMaterial(blockB, axis, -1);
                    }

                    mask[static_cast<std::size_t>(i + width * j)] = face;
                }
            }

            for (int j = 0; j < height; ++j) {
                for (int i = 0; i < width;) {
                    FaceKey key = mask[static_cast<std::size_t>(i + width * j)];
                    if (!key.visible) {
                        ++i;
                        continue;
                    }

                    int rectangleWidth = 1;
                    while (i + rectangleWidth < width &&
                           mask[static_cast<std::size_t>(i + rectangleWidth + width * j)] == key) {
                        ++rectangleWidth;
                    }

                    int rectangleHeight = 1;
                    bool canGrow = true;
                    while (j + rectangleHeight < height && canGrow) {
                        for (int k = 0; k < rectangleWidth; ++k) {
                            if (!(mask[static_cast<std::size_t>(i + k + width * (j + rectangleHeight))] == key)) {
                                canGrow = false;
                                break;
                            }
                        }
                        if (canGrow) ++rectangleHeight;
                    }

                    std::array<int, 3> origin{};
                    origin[axis] = slice + 1;
                    origin[u] = i;
                    origin[v] = j;

                    std::array<int, 3> du{};
                    std::array<int, 3> dv{};
                    du[u] = rectangleWidth;
                    dv[v] = rectangleHeight;
                    emitQuad(mesh, origin, du, dv, axis, key.sign, key.material);

                    for (int y = 0; y < rectangleHeight; ++y) {
                        for (int x = 0; x < rectangleWidth; ++x) {
                            mask[static_cast<std::size_t>(i + x + width * (j + y))] = {};
                        }
                    }
                    i += rectangleWidth;
                }
            }
        }
    }

    return mesh;
}

} // namespace rf::world
