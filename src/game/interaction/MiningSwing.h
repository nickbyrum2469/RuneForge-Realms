#pragma once

#include "game/Math.h"
#include "game/character/PlayerBodyRig.h"
#include "world/WorldEdit.h"

#include <optional>

namespace rf::world { class FrontierWorld; }

namespace rf::game::interaction {

struct SwingPose {
    bool active{false};
    bool contactMade{false};
    float normalizedTime{};
    character::ArmPose rightArm{};
    Vec3 desiredHand{};
};

struct SwingContact {
    world::RaycastHit hit{};
};

class MiningSwing {
public:
    // Interaction reach is intentionally a game-feel value rather than the literal anatomical arm
    // length. The visible arm remains fixed-length; the strike volume begins at the solved fist and
    // extends forward during the active frame so a block can be comfortably hit without the camera
    // having to be pressed against it.
    static constexpr float interactionReach = 2.45f;
    static constexpr float fistReach = interactionReach;
    static constexpr float fistRadius = 0.115f;

    void reset() noexcept;
    [[nodiscard]] bool begin(const world::RaycastHit& target,
                             Vec3 feet,
                             bool crouching,
                             Vec3 eye,
                             Vec3 forward,
                             Vec3 right,
                             Vec3 up,
                             float durationSeconds) noexcept;
    [[nodiscard]] std::optional<SwingContact> update(float deltaSeconds,
                                                     const world::FrontierWorld& world,
                                                     Vec3 feet,
                                                     bool crouching,
                                                     Vec3 eye,
                                                     Vec3 forward,
                                                     Vec3 right,
                                                     Vec3 up) noexcept;

    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] bool contactMade() const noexcept { return contactMade_; }
    [[nodiscard]] const SwingPose& pose() const noexcept { return pose_; }
    [[nodiscard]] std::optional<world::BlockCoord> lockedBlock() const noexcept {
        return active_ && target_.hit ? std::optional<world::BlockCoord>{target_.block} : std::nullopt;
    }

private:
    [[nodiscard]] Vec3 calculateDesiredHand(float t,
                                            Vec3 feet,
                                            bool crouching,
                                            Vec3 eye,
                                            Vec3 forward,
                                            Vec3 right,
                                            Vec3 up) const noexcept;
    void updatePose(float t,
                    Vec3 feet,
                    bool crouching,
                    Vec3 eye,
                    Vec3 forward,
                    Vec3 right,
                    Vec3 up) noexcept;

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
