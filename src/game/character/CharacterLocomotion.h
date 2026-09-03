#pragma once

#include <cmath>

namespace rf::game::character {

inline constexpr float gaitPhasePerMeter = 4.80f;
inline constexpr float footStrideHalfTravel = 0.195f;
inline constexpr float gaitPi = 3.14159265358979323846f;
inline constexpr float gaitTau = gaitPi * 2.0f;

[[nodiscard]] inline constexpr float locomotionPhaseFromDistance(float distanceMeters) noexcept {
    return distanceMeters * gaitPhasePerMeter;
}

[[nodiscard]] inline constexpr float plantedStrideUnitFromCycle(float cycle01) noexcept {
    if (cycle01 <= 0.0f) return -1.0f;
    if (cycle01 >= 1.0f) return -1.0f;
    if (cycle01 < 0.5f) {
        const float t = cycle01 * 2.0f;
        const float smooth = t * t * (3.0f - 2.0f * t);
        return -1.0f + smooth * 2.0f;
    }
    const float stance = (cycle01 - 0.5f) * 2.0f;
    return 1.0f - stance * 2.0f;
}

[[nodiscard]] inline constexpr float airborneLiftUnitFromCycle(float cycle01) noexcept {
    if (cycle01 <= 0.0f || cycle01 >= 0.5f) return 0.0f;
    const float t = cycle01 * 2.0f;
    const float oneMinusT = 1.0f - t;
    return 16.0f * t * t * oneMinusT * oneMinusT;
}

[[nodiscard]] inline float plantedStrideUnit(float phase) noexcept {
    float wrapped = std::fmod(phase, gaitTau);
    if (wrapped < 0.0f) wrapped += gaitTau;
    return plantedStrideUnitFromCycle(wrapped / gaitTau);
}

[[nodiscard]] inline float airborneLiftUnit(float phase) noexcept {
    float wrapped = std::fmod(phase, gaitTau);
    if (wrapped < 0.0f) wrapped += gaitTau;
    return airborneLiftUnitFromCycle(wrapped / gaitTau);
}

[[nodiscard]] inline float rightFootTravel(float phase) noexcept {
    return footStrideHalfTravel * plantedStrideUnit(phase);
}

[[nodiscard]] inline float leftFootTravel(float phase) noexcept {
    return footStrideHalfTravel * plantedStrideUnit(phase + gaitPi);
}

[[nodiscard]] inline float rightFootLiftUnit(float phase) noexcept { return airborneLiftUnit(phase); }
[[nodiscard]] inline float leftFootLiftUnit(float phase) noexcept { return airborneLiftUnit(phase + gaitPi); }

static_assert(plantedStrideUnitFromCycle(0.0f) == -1.0f);
static_assert(plantedStrideUnitFromCycle(0.5f) == 1.0f);
static_assert(plantedStrideUnitFromCycle(0.75f) == 0.0f);
static_assert(plantedStrideUnitFromCycle(1.0f) == -1.0f);
static_assert(plantedStrideUnitFromCycle(0.625f) > plantedStrideUnitFromCycle(0.750f));
static_assert(plantedStrideUnitFromCycle(0.750f) > plantedStrideUnitFromCycle(0.875f));
static_assert(airborneLiftUnitFromCycle(0.0f) == 0.0f);
static_assert(airborneLiftUnitFromCycle(0.25f) == 1.0f);
static_assert(airborneLiftUnitFromCycle(0.5f) == 0.0f);
static_assert(airborneLiftUnitFromCycle(0.75f) == 0.0f);
static_assert(airborneLiftUnitFromCycle(0.125f) == airborneLiftUnitFromCycle(0.375f));

} // namespace rf::game::character
