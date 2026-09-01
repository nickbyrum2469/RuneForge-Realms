#include "render/culling/ChunkVisibility.h"

#include "world/VoxelChunk.h"

#include <algorithm>
#include <cmath>

namespace rf::render::culling {

bool ChunkVisibility::visible(world::ChunkCoord coord,
                              game::Vec3 eye,
                              game::Vec3 forward,
                              float maxDistance,
                              float horizontalFovRadians) noexcept {
    constexpr float halfChunk = static_cast<float>(world::VoxelChunk::sizeX) * 0.5f;
    constexpr float centerY = static_cast<float>(world::VoxelChunk::sizeY) * 0.5f;
    constexpr float sphereRadius = 13.9f;

    const game::Vec3 center{
        static_cast<float>(coord.x * world::VoxelChunk::sizeX) + halfChunk,
        centerY,
        static_cast<float>(coord.z * world::VoxelChunk::sizeZ) + halfChunk,
    };
    const game::Vec3 delta = center - eye;
    const float distanceSq = game::lengthSquared(delta);
    const float maxWithRadius = maxDistance + sphereRadius;
    if (distanceSq > maxWithRadius * maxWithRadius) return false;
    if (distanceSq <= sphereRadius * sphereRadius) return true;

    const float distance = std::sqrt(distanceSq);
    const game::Vec3 direction = delta * (1.0f / distance);
    forward = game::normalized(forward);
    const float facing = direction.x * forward.x + direction.y * forward.y + direction.z * forward.z;
    const float padding = std::asin(std::clamp(sphereRadius / distance, 0.0f, 0.95f));
    const float threshold = std::cos(horizontalFovRadians * 0.5f + padding);
    return facing >= threshold;
}

} // namespace rf::render::culling
