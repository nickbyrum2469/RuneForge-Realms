#include "game/interaction/MiningSwing.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <array>
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
        Vec3 flat{forward.z, 0.0f, -forward.x};
        right = normalized(flat);
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

std::optional<world::RaycastHit> raycastFistVolume(const world::FrontierWorld& world,
                                                   Vec3 origin,
                                                   Vec3 direction,
                                                   float reach,
                                                   Vec3 right,
                                                   Vec3 up) noexcept {
    if (reach <= 0.001f) return std::nullopt;
    direction = normalized(direction);
    if (lengthSquared(direction) <= 0.000001f) return std::nullopt;

    const std::array<Vec3, 9> offsets{{
        {},
        right * MiningSwing::fistRadius,
        right * -MiningSwing::fistRadius,
        up * MiningSwing::fistRadius,
        up * -MiningSwing::fistRadius,
        (right + up) * (MiningSwing::fistRadius * 0.62f),
        (right - up) * (MiningSwing::fistRadius * 0.62f),
        (right * -1.0f + up) * (MiningSwing::fistRadius * 0.62f),
        (right * -1.0f - up) * (MiningSwing::fistRadius * 0.62f),
    }};

    std::optional<world::RaycastHit> nearest;
    float nearestDistance = reach + 1.0f;
    for (const Vec3 offset : offsets) {
        const Vec3 start = origin + offset;
        const auto hit = world.raycast(start.x, start.y, start.z,
                                       direction.x, direction.y, direction.z,
                                       reach);
        if (!hit.hit) continue;
        const Vec3 point{hit.worldX, hit.worldY, hit.worldZ};
        const float d = distance(start, point);
        if (d < nearestDistance) {
            nearestDistance = d;
            nearest = hit;
        }
    }
    return nearest;
}

} // namespace

void MiningSwing::reset() noexcept {
    active_ = false;
    contactMade_ = false;
    elapsed_ = 0.0f;
    target_ = {};
    targetPoint_ = {};
    previousHand_ = {};
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
    if (target_.hit) {
        const Vec3 contact{target.worldX, target.worldY, target.worldZ};
        if (distance(eye, contact) <= interactionReach) targetPoint_ = contact;
        else target_ = {};
    }
    if (!target_.hit) targetPoint_ = eye + forward * interactionReach;

    active_ = true;
    contactMade_ = false;
    elapsed_ = 0.0f;
    duration_ = std::clamp(durationSeconds, 0.34f, 1.20f);
    updatePose(0.0f, feet, crouching, eye, forward, right, up);
    previousHand_ = pose_.rightArm.hand;
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

    // The visible knuckles aim at the center camera ray at a reachable view-model depth. Using the
    // actual distant block face as the IK target caused the rig to clamp off-axis, which is why the
    // old punch appeared to come from underneath the player and miss the crosshair.
    const Vec3 centerStrike = eye + forward * 0.50f - up * 0.010f;
    const Vec3 windup = rest - forward * 0.12f + right * 0.20f + up * 0.10f;
    const Vec3 strike = centerStrike - right * 0.015f;
    const Vec3 follow = eye + forward * 0.55f - right * 0.13f - up * 0.085f;

    if (t < 0.20f) {
        const float u = smooth01(t / 0.20f);
        return rest * (1.0f - u) + windup * u;
    }
    if (t < 0.58f) {
        const float u = smooth01((t - 0.20f) / 0.38f);
        const float arc = std::sin(u * 3.14159265f);
        // A shoulder-height diagonal swipe gives the punch a readable wind-up instead of rising from
        // directly below the camera. The strike converges on the center cursor near the active frame.
        return windup * (1.0f - u) + strike * u + up * (arc * 0.105f) - right * (arc * 0.070f);
    }
    if (t < 0.76f) {
        const float u = smooth01((t - 0.58f) / 0.18f);
        return strike * (1.0f - u) + follow * u;
    }
    const float u = smooth01((t - 0.76f) / 0.24f);
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
    pose_.normalizedTime = std::clamp(t, 0.0f, 1.0f);
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
    const Vec3 hand = pose_.rightArm.hand;

    std::optional<SwingContact> contact;
    const bool strikeWindow = t >= 0.20f && previousT <= 0.74f;
    if (!contactMade_ && strikeWindow) {
        const Vec3 segment = hand - previousHand_;
        const float segmentLength = std::sqrt(lengthSquared(segment));

        // First test the actual swept fist motion.
        if (segmentLength > 0.0005f) {
            const Vec3 swingDirection = segment * (1.0f / segmentLength);
            if (const auto hit = raycastFistVolume(world, previousHand_, swingDirection,
                                                   segmentLength + fistRadius * 0.9f, right, up)) {
                contactMade_ = true;
                pose_.contactMade = true;
                contact = SwingContact{*hit};
            }
        }

        // Comfortable survival-game reach: during the active impact portion only, continue the
        // knuckle volume forward from the solved hand toward the crosshair. Damage still originates
        // from the embodied fist and remains one-contact-per-swing; the camera ray alone never hits.
        if (!contactMade_ && t >= 0.38f && t <= 0.68f) {
            const float handFromEye = distance(eye, hand);
            const float forwardReach = std::max(interactionReach - handFromEye, 0.0f);
            if (const auto hit = raycastFistVolume(world, hand, forward,
                                                   forwardReach, right, up)) {
                contactMade_ = true;
                pose_.contactMade = true;
                contact = SwingContact{*hit};
            }
        }
    }

    previousHand_ = hand;
    if (t >= 1.0f) {
        active_ = false;
        pose_.active = false;
    }
    return contact;
}

} // namespace rf::game::interaction
