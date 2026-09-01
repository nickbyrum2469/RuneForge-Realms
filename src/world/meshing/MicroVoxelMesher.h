#pragma once

#include "world/GreedyMesher.h"

namespace rf::world::meshing {

class MicroVoxelMesher {
public:
    static void append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh);
};

} // namespace rf::world::meshing
