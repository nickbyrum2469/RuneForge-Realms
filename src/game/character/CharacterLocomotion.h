#pragma once

namespace rf::game::character {

// Convert collision-resolved ground travel into the shared gait/viewmodel phase.
// A full cycle covers ~1.31 m of actual travel (2*pi / 4.8), close to the
// reference hero's planted athletic stride and substantially reducing visible
// foot drift versus the previous 3.31 m cycle.
inline constexpr float gaitPhasePerMeter = 4.80f;

// The ankle needs to travel backward relative to the actor root while its foot is in stance.
// At mid-stance, local ankle velocity is approximately -footStrideHalfTravel *
// gaitPhasePerMeter per meter of root travel. 0.195 m therefore cancels ~93.6% of root
// translation at the planted instant instead of the old 0.105 m arc cancelling only ~50.4%.
// This remains comfortably inside the fixed 0.42 m thigh + 0.40 m shin reach envelope.
inline constexpr float footStrideHalfTravel = 0.195f;

[[nodiscard]] inline constexpr float locomotionPhaseFromDistance(float distanceMeters) noexcept {
    return distanceMeters * gaitPhasePerMeter;
}

[[nodiscard]] inline constexpr float midStanceFootSlipFraction() noexcept {
    return 1.0f - footStrideHalfTravel * gaitPhasePerMeter;
}

} // namespace rf::game::character
