#pragma once

#include <cmath>

namespace rf::game::character {

// Convert collision-resolved ground travel into the shared gait/viewmodel phase.
// A full cycle covers ~1.31 m of actual travel (2*pi / 4.8), close to the
// reference hero's planted athletic stride and substantially reducing visible
// foot drift versus the previous 3.31 m cycle.
inline constexpr float gaitPhasePerMeter = 4.80f;

// The ankle travels backward relative to the actor root while its foot is in stance.
// Keep the amplitude established by the prior foot-skate refinement; this pass changes
// only the path shape so the planted half-cycle remains monotonic instead of reversing.
inline constexpr float footStrideHalfTravel = 0.195f;
inline constexpr float gaitPi = 3.14159265358979323846f;
inline constexpr float gaitTau = gaitPi * 2.0f;

[[nodiscard]] inline constexpr float locomotionPhaseFromDistance(float distanceMeters) noexcept {
    return distanceMeters * gaitPhasePerMeter;
}

// One normalized foot cycle, expressed as local fore/aft travel in [-1, +1].
// 0.0 -> 0.5 is the airborne recovery: ease from the rear extreme to the front extreme.
// 0.5 -> 1.0 is stance: move monotonically backward at a constant local rate while the
// actor root advances. The previous sin(phase) path returned toward center during the
// second half of stance, making the planted foot visibly scrub forward before toe-off.
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

[[nodiscard]] inline float plantedStrideUnit(float phase) noexcept {
    float wrapped = std::fmod(phase, gaitTau);
    if (wrapped < 0.0f) wrapped += gaitTau;
    return plantedStrideUnitFromCycle(wrapped / gaitTau);
}

[[nodiscard]] inline float rightFootTravel(float phase) noexcept {
    return footStrideHalfTravel * plantedStrideUnit(phase);
}

[[nodiscard]] inline float leftFootTravel(float phase) noexcept {
    return footStrideHalfTravel * plantedStrideUnit(phase + gaitPi);
}

// Compile-time gait-shape regression: recovery and stance must join continuously at
// their extrema, and the planted half-cycle must progress front -> center -> rear.
static_assert(plantedStrideUnitFromCycle(0.0f) == -1.0f);
static_assert(plantedStrideUnitFromCycle(0.5f) == 1.0f);
static_assert(plantedStrideUnitFromCycle(0.75f) == 0.0f);
static_assert(plantedStrideUnitFromCycle(1.0f) == -1.0f);
static_assert(plantedStrideUnitFromCycle(0.625f) > plantedStrideUnitFromCycle(0.750f));
static_assert(plantedStrideUnitFromCycle(0.750f) > plantedStrideUnitFromCycle(0.875f));

} // namespace rf::game::character
