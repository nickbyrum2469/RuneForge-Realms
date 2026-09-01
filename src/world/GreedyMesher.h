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

// Owns an immutable chunk snapshot plus the horizontal neighborhood needed for safe
// background meshing. Persistent damaged blocks and temporary full-resolution halo blocks
// live in microBlocks. The same 8x8x8 visual grid is used before and after damage so a hit
// never causes an obvious resolution switch.
struct ChunkMeshingSnapshot {
    VoxelChunk center;
    std::optional<VoxelChunk> negativeX;
    std::optional<VoxelChunk> positiveX;
    std::optional<VoxelChunk> negativeZ;
    std::optional<VoxelChunk> positiveZ;
    std::vector<micro::MicroBlockSnapshot> microBlocks;
    std::uint32_t worldSeed{};
    float worldAgeSeconds{};
};

class GreedyMesher {
public:
    [[nodiscard]] static VoxelMesh build(const VoxelChunk& chunk);
    [[nodiscard]] static VoxelMesh build(const ChunkMeshingSnapshot& snapshot);
};

} // namespace rf::world
