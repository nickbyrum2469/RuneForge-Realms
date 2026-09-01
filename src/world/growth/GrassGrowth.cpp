#include "world/growth/GrassGrowth.h"

#include <algorithm>
#include <cmath>

namespace rf::world::growth {
namespace {

std::uint32_t hashNode(std::uint32_t seed, BlockCoord block, int nodeX, int nodeZ) noexcept {
    std::uint32_t h = seed ^ 0x9e3779b9u;
    h ^= static_cast<std::uint32_t>(block.x) * 0x85ebca6bu;
    h ^= static_cast<std::uint32_t>(block.y) * 0xc2b2ae35u;
    h ^= static_cast<std::uint32_t>(block.z) * 0x27d4eb2fu;
    h ^= static_cast<std::uint32_t>(nodeX + nodeZ * 11) * 0x165667b1u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float unit(std::uint32_t value) noexcept {
    return static_cast<float>(value & 0xffffu) / 65535.0f;
}

} // namespace

GrowthNode GrassGrowth::sample(std::uint32_t worldSeed, BlockCoord block,
                               int nodeX, int nodeZ, float worldAgeSeconds) noexcept {
    GrowthNode node;
    if (nodeX < 0 || nodeZ < 0 || nodeX >= nodeResolution || nodeZ >= nodeResolution) return node;

    const std::uint32_t h = hashNode(worldSeed, block, nodeX, nodeZ);
    // Reference grass is patchy, not a full 8x8 carpet. About 27% of eligible nodes exist before LOD.
    node.present = unit(h) > 0.73f;
    if (!node.present) return node;

    // Quantize simulation time before sampling. A mining remesh between vegetation ticks therefore
    // produces the exact same grass state and can never make a plant grow just because the user clicked.
    const float tickAge = std::floor(std::max(worldAgeSeconds, 0.0f) / growthStepSeconds) * growthStepSeconds;
    const float initialAge = unit(h >> 7) * 76.0f;
    const float nodeStep = 18.0f + unit(h >> 13) * 30.0f;
    const int maxStage = 1 + static_cast<int>((h >> 19) % 4u);
    const int stage = std::clamp(static_cast<int>((tickAge + initialAge) / nodeStep), 0, maxStage);
    node.stage = static_cast<std::uint8_t>(stage);
    if (stage == 0) return node;

    node.height = 0.025f + static_cast<float>(stage) * 0.026f + unit(h >> 16) * 0.020f;
    node.width = 0.014f + unit(h >> 21) * 0.016f;

    if (stage >= 3 && maxStage >= 3) {
        const std::uint32_t flowerRoll = (h >> 4) % 71u;
        if (flowerRoll == 0u) node.flower = FlowerType::White;
        else if (flowerRoll == 1u) node.flower = FlowerType::Yellow;
        else if (flowerRoll == 2u && stage >= 4) node.flower = FlowerType::Blue;
    }
    return node;
}

} // namespace rf::world::growth
