#pragma once

#include "game/Math.h"
#include "world/chunks/ChunkCoord.h"

namespace rf::render::scene {

struct ChunkCullInput {
    game::Vec3 eye{};
    game::Vec3 forward{0.0f, 0.0f, 1.0f};
    float maxDistanceBlocks{104.0f};
};

class ChunkCulling {
public:
    [[nodiscard]] static bool visible(world::ChunkCoord coord, const ChunkCullInput& input) noexcept;
};

} // namespace rf::render::scene
