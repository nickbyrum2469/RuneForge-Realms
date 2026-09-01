#pragma once

#include "world/VoxelChunk.h"

#include <cstdint>
#include <optional>
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

// Owns the center chunk plus the four horizontal neighbors needed to decide whether
// boundary faces are actually visible. Copies are intentional: worker-thread meshing
// can safely consume this snapshot after the live world has streamed or changed.
struct ChunkMeshingSnapshot {
    VoxelChunk center;
    std::optional<VoxelChunk> negativeX;
    std::optional<VoxelChunk> positiveX;
    std::optional<VoxelChunk> negativeZ;
    std::optional<VoxelChunk> positiveZ;
};

class GreedyMesher {
public:
    [[nodiscard]] static VoxelMesh build(const VoxelChunk& chunk);
    [[nodiscard]] static VoxelMesh build(const ChunkMeshingSnapshot& snapshot);
};

} // namespace rf::world
