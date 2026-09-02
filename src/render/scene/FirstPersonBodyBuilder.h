#pragma once

#include "game/Math.h"
#include "game/interaction/MiningSwing.h"
#include "world/GreedyMesher.h"

namespace rf::render::scene {

class FirstPersonBodyBuilder {
public:
    [[nodiscard]] static world::VoxelMesh build(game::Vec3 eye,
                                                game::Vec3 forward,
                                                game::Vec3 right,
                                                game::Vec3 up,
                                                const game::interaction::SwingPose& swingPose);
};

} // namespace rf::render::scene
