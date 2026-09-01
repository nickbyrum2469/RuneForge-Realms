#include "game/mining/MiningCadence.h"

#include <algorithm>

namespace rf::game::mining {

bool MiningCadence::update(float deltaSeconds, float strikeIntervalSeconds) noexcept {
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.25f);
    cooldownSeconds_ = std::max(0.0f, cooldownSeconds_ - dt);
    if (!held_ || cooldownSeconds_ > 0.0f) return false;

    cooldownSeconds_ = std::clamp(strikeIntervalSeconds, 0.08f, 4.0f);
    return true;
}

} // namespace rf::game::mining
