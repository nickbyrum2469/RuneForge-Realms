#include "game/interaction/MiningSwing.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>

namespace rf::game::interaction {
namespace {

float smooth01(float value) noexcept {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float distance(Vec3 a, Vec3 b) noexcept {
    const Vec3 d = b - a;
    return std::sqrt(lengthSquared(d));
}

void canonicalizeBasis(Vec3& forward, Vec3& right, Vec3& up) noexcept {
    forward = normalized(forward);
    if (lengthSquared(forward) <= 0.000001f) forward = {0.0f, 0.0f, 1.0f};
    right = normalized(right - forward * dot(right, forward));
    if (lengthSquared(right) <= 0.000001f) {
        right = normalized({forward.z, 0.0f, -forward.x});
        if (lengthSquared(right) <= 0.000001f) right = {1.0f, 0.0f, 0.0f};
    }
    up = normalized(cross(forward, right));
    if (lengthSquared(up) <= 0.000001f) up = {0.0f, 1.0f, 0.0f};
}

Vec3 bodyForwardFromAim(Vec3 forward) noexcept {
    Vec3 bodyForward{forward.x, 0.0f, forward.z};
    bodyForward = normalized(bodyForward);
    return lengthSquared(bodyForward) > 0.000001f ? bodyForward : Vec3{0.0f, 0.0f, 1.0f};
}

} // namespace

void MiningSwing::reset() noexcept {
    active_ = false;
    contactMade_ = false;
    elapsed_ = 0.0f;
    targetDistance_ = interactionReach;
    target_ = {};
    targetPoint_ = {};
    pose_ = {};
}

bool MiningSwing::begin(const world::RaycastHit& target,
                        Vec3 feet,
                        bool crouching,
                        Vec3 eye,
                        Vec3 forward,
                        Vec3 right,
                        Vec3 up,
                        float durationSeconds) noexcept {
    if (active_) return false;
    canonicalizeBasis(forward, right, up);

    target_ = target;
    targetDistance_ = interactionReach;
    if (target_.hit) {
        const Vec3 contact{target.worldX, target.worldY, target.worldZ};
        targetDistance_ = distance(eye, contact);
        if (targetDistance_ <= interactionReach) {
            targetPoint_ = contact;
        } else {
            target_ = {};
            targetDistance_ = interactionReach;
        }
    }
    if (!target_.hit) targetPoint_ = eye + forward * interactionReach;

    active_ = true;
    contactMade_ = false;
    elapsed_ = 0.0f;
    duration_ = std::clamp(durationSeconds, 0.34f, 1.20f);
    updatePose(0.0f, feet, crouching, eye, forward, right, up);
    return true;
}

Vec3 MiningSwing::calculateDesiredHand(float t,
                                       Vec3 feet,
                                       bool crouching,
                                       Vec3 eye,
                                       Vec3 forward,
                                       Vec3 right,
                                       Vec3 up) const noexcept {
    const Vec3 bodyForward = bodyForwardFromAim(forward);
    const auto restPose = character::PlayerBodyRig::solve(feet, bodyForward, crouching);
    const Vec3 rest = restPose.rightArm.hand;

    // Third-person anatomy stays fixed-length, but the intended strike depth now responds to the
    // crosshair target distance. The first-person camera-space hand uses the same targetDistance value.
    const float targetRatio = std::clamp(targetDistance_ / interactionReach, 0.0f, 1.0f);
    const float strikeDepth = 0.47f + targetRatio * 0.18f;
    const Vec3 centerStrike = eye + forward * strikeDepth;
    const Vec3 windup = rest - forward * 0.11f + right * 0.18f + up * 0.08f;
    const Vec3 strike = centerStrike;
    const Vec3 follow = eye + forward * (strikeDepth + 0.045f) - right * 0.12f - up * 0.075f;

    if (t < 0.18f) {
        const float u = smooth01(t / 0.18f);
        return rest * (1.0f - u) + windup * u;
    }
    if (t < 0.56f) {
        const float u = smooth01((t - 0.18f) / 0.38f);
        const float arc = std::sin(u * 3.14159265f);
        return windup * (1.0f - u) + strike * u + up * (arc * 0.090f) - right * (arc * 0.055f);
    }
    if (t < 0.74f) {
        const float u = smooth01((t - 0.56f) / 0.18f);
        return strike * (1.0f - u) + follow * u;
    }
    const float u = smooth01((t - 0.74f) / 0.26f);
    return follow * (1.0f - u) + rest * u;
}

void MiningSwing::updatePose(float t,
                             Vec3 feet,
                             bool crouching,
                             Vec3 eye,
                             Vec3 forward,
                             Vec3 right,
                             Vec3 up) noexcept {
    pose_.active = active_;
    pose_.contactMade = contactMade_;
    pose_.hasTarget = target_.hit;
    pose_.normalizedTime = std::clamp(t, 0.0f, 1.0f);
    pose_.targetDistance = targetDistance_;
    pose_.desiredHand = calculateDesiredHand(t, feet, crouching, eye, forward, right, up);

    const Vec3 bodyForward = bodyForwardFromAim(forward);
    const auto bodyPose = character::PlayerBodyRig::solve(feet, bodyForward, crouching, &pose_.desiredHand);
    pose_.rightArm = bodyPose.rightArm;
}

std::optional<SwingContact> MiningSwing::update(float deltaSeconds,
                                                const world::FrontierWorld& world,
                                                Vec3 feet,
                                                bool crouching,
                                                Vec3 eye,
                                                Vec3 forward,
                                                Vec3 right,
                                                Vec3 up) noexcept {
    if (!active_) return std::nullopt;
    canonicalizeBasis(forward, right, up);

    const float previousT = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    elapsed_ += std::clamp(deltaSeconds, 0.0f, 0.08f);
    const float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    updatePose(t, feet, crouching, eye, forward, right, up);

    std::optional<SwingContact> contact;
    // 0.56 is the exact visual strike endpoint in both the third-person pose and first-person
    // viewmodel. Terrain contact therefore happens on the frame the knuckles arrive at the center
    // crosshair rather than slightly before them.
    const bool crossedImpact = previousT < 0.56f && t >= 0.56f;
    if (!contactMade_ && crossedImpact && target_.hit &&
        world.getBlock(target_.block.x, target_.block.y, target_.block.z) != world::BlockId::Air) {
        contactMade_ = true;
        pose_.contactMade = true;
        contact = SwingContact{target_};
    }

    if (t >= 1.0f) {
        active_ = false;
        pose_.active = false;
    }
    return contact;
}

} // namespace rf::game::interaction
