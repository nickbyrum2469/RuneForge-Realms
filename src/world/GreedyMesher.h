#pragma once

#include "world/VoxelChunk.h"
#include "world/WorldEdit.h"
#include "world/micro/MicroBlockSnapshot.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace rf::world {

inline constexpr std::uint32_t surfaceMaterialMask = 0x000000ffu;
inline constexpr std::uint32_t damageStageShift = 8u;
inline constexpr std::uint32_t packMaterial(SurfaceMaterial material, std::uint8_t damageStage = 0) noexcept {
    return static_cast<std::uint32_t>(material) |
           (static_cast<std::uint32_t>(damageStage) << damageStageShift);
}

struct MeshVertex {
    float x{};
    float y{};
    float z{};
    float nx{};
    float ny{};
    float nz{};
    // Low 8 bits are SurfaceMaterial. Bits 8..15 carry persistent structural damage stage.
    std::uint32_t material{};
};

struct VoxelMesh {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t quadCount{};

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
    void append(const VoxelMesh& source, float offsetX, float offsetY, float offsetZ);
};

struct DamageVisualState {
    BlockCoord position{};
    std::uint8_t stage{}; // 0 pristine, 1..5 progressive structural fracture.
};

// Owns an immutable chunk snapshot plus the horizontal neighborhood needed for safe
// background meshing. Persistent damaged blocks and temporary full-resolution halo blocks
// live in microBlocks. damageBlocks carries visual state for every structurally damaged block;
// it is independent of whichever block the player is currently targeting.
struct ChunkMeshingSnapshot {
    VoxelChunk center;
    std::optional<VoxelChunk> negativeX;
    std::optional<VoxelChunk> positiveX;
    std::optional<VoxelChunk> negativeZ;
    std::optional<VoxelChunk> positiveZ;
    std::vector<micro::MicroBlockSnapshot> microBlocks;
    std::vector<DamageVisualState> damageBlocks;
    std::uint32_t worldSeed{};
    float worldAgeSeconds{};
    int worldOriginX{};
    int worldOriginZ{};
};

class GreedyMesher {
public:
    [[nodiscard]] static VoxelMesh build(const VoxelChunk& chunk);
    [[nodiscard]] static VoxelMesh build(const ChunkMeshingSnapshot& snapshot);
};

} // namespace rf::world
