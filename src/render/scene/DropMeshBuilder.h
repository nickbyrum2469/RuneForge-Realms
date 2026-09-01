#pragma once

#include "game/drops/DropSystem.h"
#include "world/GreedyMesher.h"

#include <vector>

namespace rf::render::scene {

class DropMeshBuilder {
public:
    [[nodiscard]] static world::VoxelMesh build(const std::vector<game::drops::WorldDrop>& drops);
};

} // namespace rf::render::scene
