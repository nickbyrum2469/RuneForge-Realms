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
    if (lengthSquared(projected) <= 0.000001f) {
        projected = fallback - direction * dot(fallback, direction);
    }
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
    const Vec3 bendHint = safeNormalized(forward * 0.92f + outboard * (rightSide ? 0.18f : -0.18f),
                                         forward);
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
    const float walk = motion ? std::sin(motion->locomotionPhase) : 0.0f;
    const float idle = motion ? std::sin(motion->idlePhase) : 0.0f;
    const float gaitBob = locomotion * (0.010f + 0.008f * std::cos((motion ? motion->locomotionPhase : 0.0f) * 2.0f));
    const float idleBreath = (1.0f - locomotion) * idle;

    // Shift the pelvis over the planted leg while the opposite foot is in flight. This small lateral
    // weight transfer is what keeps the reference-heavy hero from reading like a rigid marching toy.
    // It is pose-local only: camera/world orientation is untouched and all articulated limb lengths
    // are still solved by the exact same fixed-length two-bone chains below.
    const float weightShift = -walk * locomotion * 0.018f;
    pose.pelvis = feet + pose.up * (0.82f - crouch * 0.18f + gaitBob) +
                  pose.right * weightShift - pose.forward * (crouch * 0.025f);

    // Counter-lean the upper body against the planted-leg shift. A tiny amount is enough to make the
    // head/chest stay balanced above the stance foot without introducing a cartoony side-to-side sway.
    const float torsoSideLean = walk * locomotion * 0.026f;
    const Vec3 torsoDirection = safeNormalized(pose.up * (1.0f - crouch * 0.13f) +
                                                pose.forward * (crouch * 0.34f + locomotion * 0.025f) +
                                                pose.right * torsoSideLean,
                                                pose.up);

    // Idle breathing should read through the ribcage instead of making the whole hero hover vertically.
    // Keep the pelvis planted and expand only the spine/chest stack by a few millimeters. Shoulders,
    // head and arms inherit that local torso motion while the fixed-length limb solvers remain intact.
    const float breathSpine = idleBreath * 0.0015f;
    const float breathChest = idleBreath * 0.0035f;
    const float breathForward = idleBreath * 0.0040f;
    pose.spine = pose.pelvis + torsoDirection * (0.245f + breathSpine);
    pose.chest = pose.spine + torsoDirection * (0.270f + breathChest) + pose.forward * breathForward;

    // The reference head sits down into a short, thick neck instead of floating above the deltoids.
    // Compact only this upper-neck stack: torso and articulated limb lengths stay exactly unchanged.
    pose.neck = pose.chest + torsoDirection * 0.105f;
    pose.head = pose.neck + torsoDirection * 0.125f;

    // The supplied hero reference reads as a pronounced V-taper: very broad deltoids/chest over a
    // compact pelvis. Keep that silhouette in the rig itself so locomotion cannot collapse it back
    // toward generic humanoid proportions when the arms and legs articulate.
    const float shoulderTwist = walk * locomotion * 0.022f;
    const Vec3 rightShoulder = pose.chest + pose.right * 0.315f + torsoDirection * 0.020f -
                               pose.forward * shoulderTwist;
    const Vec3 leftShoulder = pose.chest - pose.right * 0.315f + torsoDirection * 0.020f +
                              pose.forward * shoulderTwist;

    // The turnaround reference has long relaxed arms that hang beside the upper thighs rather than
    // stopping almost level with the pelvis. Lower the rest targets while keeping a slight forward
    // bias so the large hands stay readable from front/3/4 views. The fixed-length arm solver still
    // owns the anatomy, so this changes pose/silhouette without stretching either arm segment.
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

    // The pelvis rotates a few centimeters with the stride, opposite the shoulder twist. This gives
    // the broad hero a connected trunk instead of independently swinging arms hanging from a static box.
    const float hipTwist = walk * locomotion * 0.018f;
    const Vec3 rightHip = pose.pelvis + pose.right * 0.135f + pose.forward * hipTwist;
    const Vec3 leftHip = pose.pelvis - pose.right * 0.135f - pose.forward * hipTwist;
    const float crouchFootSpread = crouch * 0.045f;
    const float rightLift = locomotion * std::max(walk, 0.0f) * 0.052f;
    const float leftLift = locomotion * std::max(-walk, 0.0f) * 0.052f;

    // Keep the prior 0.195 m stride amplitude, but use a stance-aware path instead of sin(phase).
    // During the grounded half-cycle each ankle now moves monotonically front -> rear while the root
    // advances. The old sinusoid reversed direction during late stance, visibly scrubbing the planted
    // foot forward before toe-off. Recovery remains eased while that foot is lifted.
    constexpr float referenceAnkleHalfSpan = 0.165f;
    const float rightTravel = locomotion * rightFootTravel(motion ? motion->locomotionPhase : 0.0f);
    const float leftTravel = locomotion * leftFootTravel(motion ? motion->locomotionPhase : 0.0f);
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
