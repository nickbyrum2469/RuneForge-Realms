#include "game/character/PlayerBodyRig.h"

#include "game/character/CharacterLocomotion.h"

#include <algorithm>
#include <cmath>

namespace rf::game::character {
namespace {

constexpr float kEpsilon = 0.0001f;

float length(Vec3 value) noexcept {
    return std::sqrt(lengthSquared(value));
}

Vec3 safeNormalized(Vec3 value, Vec3 fallback) noexcept {
    const Vec3 result = normalized(value);
    return lengthSquared(result) > 0.000001f ? result : fallback;
}

Vec3 perpendicularToward(Vec3 direction, Vec3 hint, Vec3 fallback) noexcept {
    direction = safeNormalized(direction, {0.0f, 0.0f, 1.0f});
    Vec3 projected = hint - direction * dot(hint, direction);
    if (lengthSquared(projected) <= 0.000001f) projected = fallback - direction * dot(fallback, direction);
    return safeNormalized(projected, {0.0f, -1.0f, 0.0f});
}

struct TwoBoneResult {
    Vec3 middle{};
    Vec3 end{};
};

TwoBoneResult solveTwoBone(Vec3 start,
                           Vec3 requestedEnd,
                           Vec3 bendHint,
                           Vec3 fallbackBend,
                           float firstLength,
                           float secondLength) noexcept {
    Vec3 delta = requestedEnd - start;
    float distance = length(delta);
    Vec3 direction = distance > kEpsilon ? delta * (1.0f / distance)
                                         : safeNormalized(fallbackBend, {0.0f, 0.0f, 1.0f});

    const float minReach = std::abs(firstLength - secondLength) + 0.001f;
    const float maxReach = firstLength + secondLength - 0.001f;
    distance = std::clamp(distance, minReach, maxReach);
    const Vec3 end = start + direction * distance;

    const float along = (firstLength * firstLength - secondLength * secondLength + distance * distance) /
                        (2.0f * distance);
    const float heightSq = std::max(firstLength * firstLength - along * along, 0.0f);
    const float height = std::sqrt(heightSq);
    const Vec3 bendDirection = perpendicularToward(direction, bendHint, fallbackBend);
    return {start + direction * along + bendDirection * height, end};
}

Vec3 horizontalForward(Vec3 facingForward) noexcept {
    Vec3 flat{facingForward.x, 0.0f, facingForward.z};
    flat = normalized(flat);
    return lengthSquared(flat) > 0.000001f ? flat : Vec3{0.0f, 0.0f, 1.0f};
}

} // namespace

ArmPose PlayerBodyRig::solveArm(Vec3 shoulder,
                                Vec3 desiredHand,
                                Vec3 bendHint,
                                Vec3 handDirection,
                                bool rightSide) noexcept {
    const Vec3 fallbackDirection = rightSide ? Vec3{0.35f, -0.45f, 0.82f}
                                             : Vec3{-0.35f, -0.45f, 0.82f};
    handDirection = safeNormalized(handDirection, fallbackDirection);

    const Vec3 requestedWrist = desiredHand - handDirection * handCenterOffset;
    const TwoBoneResult solved = solveTwoBone(shoulder,
                                               requestedWrist,
                                               bendHint,
                                               {0.0f, -1.0f, 0.0f},
                                               upperArmLength,
                                               forearmLength);

    ArmPose pose;
    pose.shoulder = shoulder;
    pose.elbow = solved.middle;
    pose.wrist = solved.end;
    pose.hand = pose.wrist + handDirection * handCenterOffset;
    pose.toolSocket = pose.hand + handDirection * 0.105f;
    return pose;
}

LegPose PlayerBodyRig::solveLeg(Vec3 hip,
                                Vec3 ankle,
                                Vec3 forward,
                                Vec3 outboard,
                                bool rightSide) noexcept {
    const Vec3 bendHint = safeNormalized(forward * 0.92f + outboard * (rightSide ? 0.18f : -0.18f), forward);
    const TwoBoneResult solved = solveTwoBone(hip,
                                               ankle,
                                               bendHint,
                                               forward,
                                               thighLength,
                                               shinLength);
    LegPose pose;
    pose.hip = hip;
    pose.knee = solved.middle;
    pose.ankle = solved.end;
    pose.foot = pose.ankle + forward * 0.150f + Vec3{0.0f, 0.015f, 0.0f};
    return pose;
}

PlayerBodyPose PlayerBodyRig::solve(Vec3 feet,
                                    Vec3 facingForward,
                                    bool crouching,
                                    const Vec3* rightHandTarget,
                                    const Vec3* leftHandTarget,
                                    const BodyMotionState* motion) noexcept {
    PlayerBodyPose pose;
    pose.root = feet;
    pose.up = {0.0f, 1.0f, 0.0f};
    pose.forward = horizontalForward(facingForward);
    pose.right = safeNormalized({pose.forward.z, 0.0f, -pose.forward.x}, {1.0f, 0.0f, 0.0f});

    const float crouch = crouching ? 1.0f : 0.0f;
    const float locomotion = motion ? std::clamp(motion->locomotionAmount, 0.0f, 1.0f) : 0.0f;
    const float phase = motion ? motion->locomotionPhase : 0.0f;
    const float walk = std::sin(phase);
    const float idle = motion ? std::sin(motion->idlePhase) : 0.0f;
    const float gaitBob = locomotion * (0.010f + 0.008f * std::cos(phase * 2.0f));
    const float idleBreath = (1.0f - locomotion) * idle;

    const float weightShift = -walk * locomotion * 0.018f;
    pose.pelvis = feet + pose.up * (0.82f - crouch * 0.18f + gaitBob) +
                  pose.right * weightShift - pose.forward * (crouch * 0.025f);

    const float torsoSideLean = walk * locomotion * 0.026f;
    const Vec3 torsoDirection = safeNormalized(pose.up * (1.0f - crouch * 0.13f) +
                                                pose.forward * (crouch * 0.34f + locomotion * 0.025f) +
                                                pose.right * torsoSideLean,
                                                pose.up);

    // Breathing expands the torso instead of hovering the entire player vertically.
    const float breathSpine = idleBreath * 0.0015f;
    const float breathChest = idleBreath * 0.0035f;
    const float breathForward = idleBreath * 0.0040f;
    pose.spine = pose.pelvis + torsoDirection * (0.245f + breathSpine);
    pose.chest = pose.spine + torsoDirection * (0.270f + breathChest) + pose.forward * breathForward;
    pose.neck = pose.chest + torsoDirection * 0.105f;
    pose.head = pose.neck + torsoDirection * 0.125f;

    const float shoulderTwist = walk * locomotion * 0.022f;
    const Vec3 rightShoulder = pose.chest + pose.right * 0.315f + torsoDirection * 0.020f -
                               pose.forward * shoulderTwist;
    const Vec3 leftShoulder = pose.chest - pose.right * 0.315f + torsoDirection * 0.020f +
                              pose.forward * shoulderTwist;

    const float armSwing = walk * locomotion * 0.105f;
    const Vec3 rightRestHand = pose.pelvis + pose.right * 0.330f + pose.forward * (0.050f - armSwing) -
                               pose.up * 0.190f;
    const Vec3 leftRestHand = pose.pelvis - pose.right * 0.330f + pose.forward * (0.045f + armSwing) -
                              pose.up * 0.190f;
    const Vec3 rightDesired = rightHandTarget ? *rightHandTarget : rightRestHand;
    const Vec3 leftDesired = leftHandTarget ? *leftHandTarget : leftRestHand;

    const Vec3 rightHandDirection = safeNormalized(rightDesired - rightShoulder,
                                                    pose.forward * 0.55f - pose.up * 0.70f + pose.right * 0.25f);
    const Vec3 leftHandDirection = safeNormalized(leftDesired - leftShoulder,
                                                   pose.forward * 0.55f - pose.up * 0.70f - pose.right * 0.25f);
    pose.rightArm = solveArm(rightShoulder,
                             rightDesired,
                             pose.right * 0.58f - pose.up * 0.88f + pose.forward * 0.06f,
                             rightHandDirection,
                             true);
    pose.leftArm = solveArm(leftShoulder,
                            leftDesired,
                            pose.right * -0.58f - pose.up * 0.88f + pose.forward * 0.06f,
                            leftHandDirection,
                            false);

    const float hipTwist = walk * locomotion * 0.018f;
    const Vec3 rightHip = pose.pelvis + pose.right * 0.135f + pose.forward * hipTwist;
    const Vec3 leftHip = pose.pelvis - pose.right * 0.135f - pose.forward * hipTwist;
    const float crouchFootSpread = crouch * 0.045f;

    // The eased recovery path crosses the body center halfway through the airborne phase, while the
    // torso/arm weight-transfer signal reaches its maximum at pi/2. Lead the foot path by 30 degrees
    // so the airborne leg is visibly forward when the opposite arm and planted-leg weight transfer
    // are at their strongest. This preserves the planted monotonic stance path and zero-slope
    // toe-off/touchdown while keeping the whole gait synchronized instead of looking disconnected.
    const float footPhase = phase + gaitPi / 6.0f;
    const float rightLift = locomotion * rightFootLiftUnit(footPhase) * 0.052f;
    const float leftLift = locomotion * leftFootLiftUnit(footPhase) * 0.052f;
    constexpr float referenceAnkleHalfSpan = 0.165f;
    const float rightTravel = locomotion * rightFootTravel(footPhase);
    const float leftTravel = locomotion * leftFootTravel(footPhase);
    const Vec3 rightAnkle = feet + pose.right * (referenceAnkleHalfSpan + crouchFootSpread) +
                            pose.forward * (crouch * 0.09f + rightTravel) +
                            pose.up * (0.075f + rightLift);
    const Vec3 leftAnkle = feet - pose.right * (referenceAnkleHalfSpan + crouchFootSpread) +
                           pose.forward * (crouch * 0.09f + leftTravel) +
                           pose.up * (0.075f + leftLift);
    pose.rightLeg = solveLeg(rightHip, rightAnkle, pose.forward, pose.right, true);
    pose.leftLeg = solveLeg(leftHip, leftAnkle, pose.forward, pose.right, false);

    return pose;
}

} // namespace rf::game::character
