#include "world/growth/GrassGrowth.h"

#include <algorithm>

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
    const float density = unit(h);
    node.present = density > 0.36f;
    if (!node.present) return node;

    const float initialOffset = unit(h >> 8) * growthStepSeconds * 2.4f;
    const float effectiveAge = std::max(0.0f, worldAgeSeconds + initialOffset);
    const int stage = std::clamp(static_cast<int>(effectiveAge / growthStepSeconds), 0, 4);
    node.stage = static_cast<std::uint8_t>(stage);
    node.height = 0.035f + static_cast<float>(stage) * 0.035f + unit(h >> 16) * 0.025f;
    node.width = 0.050f + unit(h >> 21) * 0.045f;

    if (stage >= 3) {
        const std::uint32_t flowerRoll = (h >> 5) % 23u;
        if (flowerRoll == 0u) node.flower = FlowerType::White;
        else if (flowerRoll == 1u) node.flower = FlowerType::Yellow;
        else if (flowerRoll == 2u && stage >= 4) node.flower = FlowerType::Blue;
    }
    return node;
}

} // namespace rf::world::growth
