#pragma once

#include "game/Math.h"
#include "world/blocks/BlockDefinition.h"

#include <cstdint>

namespace rf::game::audio {

enum class AudioEventType : std::uint8_t {
    Footstep,
    Jump,
    Land,
    MiningHit,
    BlockBreak,
    BlockPlace,
    ItemPickup,
    InventoryMove,
    UiHover,
    UiClick,
    Craft,
    PlayerDamage,
    WaterSplash,
    Environment,
};

struct AudioEvent {
    AudioEventType type{AudioEventType::Environment};
    world::blocks::SoundFamily family{world::blocks::SoundFamily::None};
    Vec3 position{};
    float gain{1.0f};
    float pitch{1.0f};
    bool spatial{true};
    std::uint32_t variationSeed{};
};

} // namespace rf::game::audio
