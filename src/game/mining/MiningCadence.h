#pragma once

namespace rf::game::mining {

// Converts held-button input into deterministic strikes. Releasing/re-pressing the mouse never
// bypasses an active cooldown, so mining speed is independent of click frequency and frame rate.
class MiningCadence {
public:
    void press() noexcept { held_ = true; }
    void release() noexcept { held_ = false; }
    void reset() noexcept { held_ = false; cooldownSeconds_ = 0.0f; }

    [[nodiscard]] bool update(float deltaSeconds, float strikeIntervalSeconds) noexcept;
    [[nodiscard]] bool held() const noexcept { return held_; }
    [[nodiscard]] float cooldownSeconds() const noexcept { return cooldownSeconds_; }

private:
    bool held_{false};
    float cooldownSeconds_{0.0f};
};

} // namespace rf::game::mining
