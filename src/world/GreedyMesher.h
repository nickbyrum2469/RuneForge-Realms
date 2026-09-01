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

struct ChunkMesh {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t quadCount{};

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
};

class GreedyMesher {
public:
    [[nodiscard]] static ChunkMesh build(const VoxelChunk& chunk);
};

} // namespace rf::world
