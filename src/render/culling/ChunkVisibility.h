#pragma once

#include "game/Math.h"
#include "world/chunks/ChunkCoord.h"

namespace rf::render::culling {

class ChunkVisibility {
public:
    [[nodiscard]] static bool visible(world::ChunkCoord coord,
                                      game::Vec3 eye,
                                      game::Vec3 forward,
                                      float maxDistance,
                                      float horizontalFovRadians) noexcept;
};

} // namespace rf::render::culling
