#pragma once

#include "world/VoxelChunk.h"

#include <cstdint>
#include <vector>

namespace rf::world {

struct MeshVertex {
    float x{};
    float y{};
    float z{};
    float nx{};
    float ny{};
    float nz{};
    std::uint32_t material{};
};

struct VoxelMesh {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t quadCount{};

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
    void append(const VoxelMesh& source, float offsetX, float offsetY, float offsetZ);
};

class GreedyMesher {
public:
    [[nodiscard]] static VoxelMesh build(const VoxelChunk& chunk);
};

} // namespace rf::world
