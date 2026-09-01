#pragma once

#include "world/GreedyMesher.h"

#include <cstdint>

namespace rf::world::meshing {

enum class SurfaceDetailTier : std::uint8_t {
    Distant = 0,
    Standard = 1,
    Hero = 2,
};

// Adds deterministic, non-colliding geometry to exposed block surfaces. Physical damage is
// always represented by MicroVoxelMesher; this layer supplies the dense pristine silhouette
// language (grass nodes, stone plates, leaf clumps) used by the reference-quality materials.
// Tiers are selected by camera distance only. A mining strike never changes a block's detail tier.
class MicroDetailBuilder {
public:
    static void append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                       SurfaceDetailTier tier = SurfaceDetailTier::Hero);
};

} // namespace rf::world::meshing
