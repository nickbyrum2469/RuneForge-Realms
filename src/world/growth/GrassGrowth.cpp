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

    // Grass blocks are authored as a continuous turf carpet. Every one of the 8x8 top-surface cells
    // owns a short blade cluster from first load; growth changes subtle maturity/height, never whether
    // a patch exists. This removes bald checkerboard holes and prevents mining/remeshing from appearing
    // to "grow" grass that should already be part of the material silhouette.
    node.present = true;

    const float tickAge = std::floor(std::max(worldAgeSeconds, 0.0f) / growthStepSeconds) * growthStepSeconds;
    const int baseStage = 2 + static_cast<int>((h >> 18) & 1u);
    const int maxStage = 3 + static_cast<int>((h >> 22) & 1u);
    const float matureAfter = 48.0f + unit(h >> 9) * 96.0f;
    const int ageBonus = tickAge >= matureAfter ? 1 : 0;
    const int stage = std::clamp(baseStage + ageBonus, 2, maxStage);

    node.stage = static_cast<std::uint8_t>(stage);
    // Keep the carpet short and visually uniform. Variation is intentionally only a few centimeters
    // so the reference reads as dense trimmed turf instead of scattered tall needles.
    node.height = 0.060f + static_cast<float>(stage - 2) * 0.010f + unit(h >> 15) * 0.015f;
    node.width = 0.019f + unit(h >> 21) * 0.006f;

    if (stage >= 3) {
        const std::uint32_t flowerRoll = (h >> 4) % 127u;
        if (flowerRoll == 0u) node.flower = FlowerType::White;
        else if (flowerRoll == 1u) node.flower = FlowerType::Yellow;
        else if (flowerRoll == 2u && stage >= 4) node.flower = FlowerType::Blue;
    }
    return node;
}

} // namespace rf::world::growth
