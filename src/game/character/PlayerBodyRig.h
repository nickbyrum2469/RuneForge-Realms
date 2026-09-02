#pragma once

#include "game/Math.h"

namespace rf::game::character {

struct ArmPose {
    Vec3 shoulder{};
    Vec3 elbow{};
    Vec3 wrist{};
    Vec3 hand{};
    Vec3 toolSocket{};
};

struct LegPose {
    Vec3 hip{};
    Vec3 knee{};
    Vec3 ankle{};
    Vec3 foot{};
};

struct PlayerBodyPose {
    Vec3 root{};
    Vec3 pelvis{};
    Vec3 spine{};
    Vec3 chest{};
    Vec3 neck{};
    Vec3 head{};
    Vec3 forward{0.0f, 0.0f, 1.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    ArmPose rightArm{};
    ArmPose leftArm{};
    LegPose rightLeg{};
    LegPose leftLeg{};
};

class PlayerBodyRig {
public:
    static constexpr float referenceHeight = 1.80f;
    static constexpr float upperArmLength = 0.34f;
    static constexpr float forearmLength = 0.31f;
    static constexpr float handCenterOffset = 0.075f;
    static constexpr float thighLength = 0.42f;
    static constexpr float shinLength = 0.40f;

    [[nodiscard]] static PlayerBodyPose solve(Vec3 feet,
                                              Vec3 facingForward,
                                              bool crouching = false,
                                              const Vec3* rightHandTarget = nullptr,
                                              const Vec3* leftHandTarget = nullptr) noexcept;

    [[nodiscard]] static ArmPose solveArm(Vec3 shoulder,
                                          Vec3 desiredHand,
                                          Vec3 bendHint,
                                          Vec3 handDirection,
                                          bool rightSide) noexcept;

private:
    [[nodiscard]] static LegPose solveLeg(Vec3 hip,
                                          Vec3 ankle,
                                          Vec3 forward,
                                          Vec3 outboard,
                                          bool rightSide) noexcept;
};

} // namespace rf::game::character
