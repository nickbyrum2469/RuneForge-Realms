#pragma once

#include "world/WorldEdit.h"

#include <cstdint>

namespace rf::world::growth {

enum class FlowerType : std::uint8_t {
    None = 0,
    White,
    Yellow,
    Blue,
};

struct GrowthNode {
    bool present{false};
    std::uint8_t stage{};
    float height{};
    float width{};
    FlowerType flower{FlowerType::None};
};

class GrassGrowth {
public:
    static constexpr int nodeResolution = 8;
    // World vegetation is reconsidered at a coarse random-tick cadence, never from player input.
    static constexpr float growthStepSeconds = 8.0f;

    [[nodiscard]] static GrowthNode sample(std::uint32_t worldSeed, BlockCoord block,
                                           int nodeX, int nodeZ, float worldAgeSeconds) noexcept;
};

} // namespace rf::world::growth
