#include "render/scene/ChunkCulling.h"

#include "world/VoxelChunk.h"

#include <algorithm>
#include <cmath>

namespace rf::render::scene {

bool ChunkCulling::visible(world::ChunkCoord coord, const ChunkCullInput& input) noexcept {
    const float minX = static_cast<float>(coord.x * world::VoxelChunk::sizeX);
    const float minZ = static_cast<float>(coord.z * world::VoxelChunk::sizeZ);
    const float maxX = minX + static_cast<float>(world::VoxelChunk::sizeX);
    const float maxZ = minZ + static_cast<float>(world::VoxelChunk::sizeZ);

    // Distance cull the chunk footprint, not just its center, so an edge/corner still inside the
    // render radius cannot disappear and create a scalloped horizon hole.
    const float nearestX = std::clamp(input.eye.x, minX, maxX);
    const float nearestZ = std::clamp(input.eye.z, minZ, maxZ);
    const float nearestDx = nearestX - input.eye.x;
    const float nearestDz = nearestZ - input.eye.z;
    if (nearestDx * nearestDx + nearestDz * nearestDz >
        input.maxDistanceBlocks * input.maxDistanceBlocks) return false;

    const float centerX = minX + world::VoxelChunk::sizeX * 0.5f;
    const float centerZ = minZ + world::VoxelChunk::sizeZ * 0.5f;
    const float dx = centerX - input.eye.x;
    const float dz = centerZ - input.eye.z;
    const float distanceSq = dx * dx + dz * dz;
    if (distanceSq <= 24.0f * 24.0f) return true;

    const float distance = std::sqrt(distanceSq);
    const float forwardLength = std::sqrt(input.forward.x * input.forward.x + input.forward.z * input.forward.z);
    if (distance <= 0.0001f || forwardLength <= 0.0001f) return true;

    const float dot = (dx / distance) * (input.forward.x / forwardLength) +
                      (dz / distance) * (input.forward.z / forwardLength);
    return dot > -0.35f;
}

} // namespace rf::render::scene
