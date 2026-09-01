#pragma once

#include "world/VoxelChunk.h"
#include "world/micro/MicroBlockSnapshot.h"

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

// Owns the center chunk plus horizontal neighbors needed for safe background meshing.
// `microBlocks` contains only promoted/damaged blocks. Their cells are removed from the
// coarse center copy so MicroVoxelMesher can rebuild them at 1/8-block resolution.
struct ChunkMeshingSnapshot {
    VoxelChunk center;
    std::optional<VoxelChunk> negativeX;
    std::optional<VoxelChunk> positiveX;
    std::optional<VoxelChunk> negativeZ;
    std::optional<VoxelChunk> positiveZ;
    std::vector<micro::MicroBlockSnapshot> microBlocks;
};

class GreedyMesher {
public:
    [[nodiscard]] static VoxelMesh build(const VoxelChunk& chunk);
    [[nodiscard]] static VoxelMesh build(const ChunkMeshingSnapshot& snapshot);
};

} // namespace rf::world
