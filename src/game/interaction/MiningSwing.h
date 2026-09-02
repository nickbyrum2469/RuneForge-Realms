#pragma once

#include "game/Math.h"
#include "world/WorldEdit.h"

#include <optional>

namespace rf::world { class FrontierWorld; }

namespace rf::game::interaction {

struct SwingPose {
    bool active{false};
    bool contactMade{false};
    float normalizedTime{};
    Vec3 shoulder{};
    Vec3 elbow{};
    Vec3 hand{};
};

struct SwingContact {
    world::RaycastHit hit{};
};

class MiningSwing {
public:
    static constexpr float fistReach = 1.72f;
    static constexpr float fistRadius = 0.11f;

    void reset() noexcept;
    [[nodiscard]] bool begin(const world::RaycastHit& target, Vec3 eye, Vec3 forward, Vec3 right, Vec3 up,
                             float durationSeconds) noexcept;
    [[nodiscard]] std::optional<SwingContact> update(float deltaSeconds, const world::FrontierWorld& world,
                                                     Vec3 eye, Vec3 forward, Vec3 right, Vec3 up) noexcept;

    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] bool contactMade() const noexcept { return contactMade_; }
    [[nodiscard]] const SwingPose& pose() const noexcept { return pose_; }
    [[nodiscard]] std::optional<world::BlockCoord> lockedBlock() const noexcept {
        return active_ ? std::optional<world::BlockCoord>{target_.block} : std::nullopt;
    }

private:
    [[nodiscard]] Vec3 calculateHand(float t, Vec3 eye, Vec3 forward, Vec3 right, Vec3 up) const noexcept;
    void updatePose(float t, Vec3 eye, Vec3 forward, Vec3 right, Vec3 up) noexcept;

    bool active_{false};
    bool contactMade_{false};
    float elapsed_{};
    float duration_{0.52f};
    world::RaycastHit target_{};
    Vec3 targetPoint_{};
    Vec3 previousHand_{};
    SwingPose pose_{};
};

} // namespace rf::game::interaction
