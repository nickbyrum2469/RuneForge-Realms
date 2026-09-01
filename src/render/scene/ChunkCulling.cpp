#include "render/scene/ChunkCulling.h"

#include "world/VoxelChunk.h"

#include <cmath>

namespace rf::render::scene {

bool ChunkCulling::visible(world::ChunkCoord coord, const ChunkCullInput& input) noexcept {
    const float centerX = static_cast<float>(coord.x * world::VoxelChunk::sizeX) + world::VoxelChunk::sizeX * 0.5f;
    const float centerZ = static_cast<float>(coord.z * world::VoxelChunk::sizeZ) + world::VoxelChunk::sizeZ * 0.5f;
    const float dx = centerX - input.eye.x;
    const float dz = centerZ - input.eye.z;
    const float distanceSq = dx * dx + dz * dz;
    if (distanceSq > input.maxDistanceBlocks * input.maxDistanceBlocks) return false;

    // Chunks close to the player are always kept to avoid edge popping while turning.
    if (distanceSq <= 24.0f * 24.0f) return true;

    const float length = std::sqrt(distanceSq);
    if (length <= 0.0001f) return true;
    const float forwardLength = std::sqrt(input.forward.x * input.forward.x + input.forward.z * input.forward.z);
    if (forwardLength <= 0.0001f) return true;
    const float dot = (dx / length) * (input.forward.x / forwardLength) +
                      (dz / length) * (input.forward.z / forwardLength);
    return dot > -0.35f;
}

} // namespace rf::render::scene
