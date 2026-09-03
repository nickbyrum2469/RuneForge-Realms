#pragma once

#include "world/GreedyMesher.h"

#include <cstdint>

namespace rf::world::meshing {

enum class SurfaceDetailTier : std::uint8_t {
    Distant = 0,
    Standard = 1,
    Hero = 2,
};

struct SurfaceDetailStats {
    std::uint32_t grassBlocks{};
    std::uint32_t dirtBlocks{};
    std::uint32_t promotedGrassBlocks{};
    std::uint32_t topReliefCells{};
    std::uint32_t sideReliefCells{};
    std::uint32_t grassClusters{};
    std::uint32_t rootCells{};
    std::uint32_t cavityCells{};
};

// Converts deterministic world-owned surface fields into bounded render geometry. The surface field
// is shared by pristine and promoted blocks; physical damage remains authoritative in MicroVoxelMesher.
// A mining/remesh event therefore cannot invent a new grass layout or downgrade the material language.
class MicroDetailBuilder {
public:
    static void append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                       SurfaceDetailTier tier = SurfaceDetailTier::Hero,
                       SurfaceDetailStats* stats = nullptr);
};

} // namespace rf::world::meshing
