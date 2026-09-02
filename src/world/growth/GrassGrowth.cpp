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

    // The 0.5.2 turf looked sparse and could appear to "grow" only after a mining remesh because a
    // large portion of candidate nodes started at stage zero. Grass is now part of the authored
    // surface silhouette: a deterministic majority of nodes exist immediately when the chunk loads.
    // Random-tick age may mature a tuft slightly, but player input can never create or reveal it.
    node.present = unit(h) > 0.43f; // ~57% of the 8x8 node field before distance LOD.
    if (!node.present) return node;

    const float tickAge = std::floor(std::max(worldAgeSeconds, 0.0f) / growthStepSeconds) * growthStepSeconds;
    const int baseStage = 2 + static_cast<int>((h >> 18) & 1u); // Every visible tuft starts established.
    const int maxStage = 3 + static_cast<int>((h >> 22) & 1u);
    const float matureAfter = 48.0f + unit(h >> 9) * 96.0f;
    const int ageBonus = tickAge >= matureAfter ? 1 : 0;
    const int stage = std::clamp(baseStage + ageBonus, 2, maxStage);

    node.stage = static_cast<std::uint8_t>(stage);
    node.height = 0.052f + static_cast<float>(stage) * 0.022f + unit(h >> 15) * 0.030f;
    node.width = 0.020f + unit(h >> 21) * 0.016f;

    // Flowers stay uncommon accents. They are deterministic from seed and do not pop into existence
    // because the player happens to punch or remesh a nearby block.
    if (stage >= 3) {
        const std::uint32_t flowerRoll = (h >> 4) % 97u;
        if (flowerRoll == 0u) node.flower = FlowerType::White;
        else if (flowerRoll == 1u) node.flower = FlowerType::Yellow;
        else if (flowerRoll == 2u && stage >= 4) node.flower = FlowerType::Blue;
    }
    return node;
}

} // namespace rf::world::growth
