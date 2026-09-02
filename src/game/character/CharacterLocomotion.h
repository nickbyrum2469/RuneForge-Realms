#pragma once

namespace rf::game::character {

// Convert collision-resolved ground travel into the shared gait/viewmodel phase.
// A full cycle covers ~1.31 m of actual travel (2*pi / 4.8), close to the
// reference hero's planted athletic stride and substantially reducing visible
// foot drift versus the previous 3.31 m cycle.
inline constexpr float gaitPhasePerMeter = 4.80f;

[[nodiscard]] inline constexpr float locomotionPhaseFromDistance(float distanceMeters) noexcept {
    return distanceMeters * gaitPhasePerMeter;
}

} // namespace rf::game::character
