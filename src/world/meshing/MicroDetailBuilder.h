#pragma once

#include "world/GreedyMesher.h"

namespace rf::world::meshing {

// Adds deterministic, non-colliding decorative geometry to exposed block surfaces.
// Gameplay still sees one logical voxel; this exists only to break silhouettes and create
// the dense turf/rock/foliage character of RuneForge's hero material references.
class MicroDetailBuilder {
public:
    static void append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh);
};

} // namespace rf::world::meshing
