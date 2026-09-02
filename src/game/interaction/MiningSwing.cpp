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
        if (distance(eye, contact) <= fistReach) targetPoint_ = contact;
        else target_ = {};
    }
    if (!target_.hit) {
        // Empty-air swings still have a deliberate side/diagonal attack arc. A camera ray is only
        // intent nomination; it is never a prerequisite for animation or contact.
        targetPoint_ = eye + forward * 0.86f - up * 0.08f - right * 0.03f;
    }

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
    (void)eye;
    const Vec3 bodyForward = bodyForwardFromAim(forward);
    const auto restPose = character::PlayerBodyRig::solve(feet, bodyForward, crouching);
    const Vec3 rest = restPose.rightArm.hand;
    const Vec3 windup = rest - forward * 0.16f + right * 0.17f + up * 0.035f;
    const Vec3 strikeAnchor = target_.hit
        ? targetPoint_ - forward * (fistRadius * 0.22f)
        : targetPoint_;
    const Vec3 strike = strikeAnchor - right * 0.055f - up * 0.035f;

    if (t < 0.24f) {
        const float u = smooth01(t / 0.24f);
        return rest * (1.0f - u) + windup * u;
    }
    if (t < 0.60f) {
        const float u = smooth01((t - 0.24f) / 0.36f);
        const float arc = std::sin(u * 3.14159265f);
        return windup * (1.0f - u) + strike * u + up * (arc * 0.075f) - right * (arc * 0.055f);
    }
    if (t < 0.78f) {
        const float u = smooth01((t - 0.60f) / 0.18f);
        const Vec3 follow = strike + forward * 0.095f - right * 0.105f - up * 0.075f;
        return strike * (1.0f - u) + follow * u;
    }
    const float u = smooth01((t - 0.78f) / 0.22f);
    const Vec3 follow = strike + forward * 0.095f - right * 0.105f - up * 0.075f;
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
    const bool strikeWindow = t >= 0.24f && previousT <= 0.76f;
    if (!contactMade_ && strikeWindow) {
        const Vec3 segment = hand - previousHand_;
        const float segmentLength = std::sqrt(lengthSquared(segment));
        if (segmentLength > 0.0005f) {
            const Vec3 direction = segment * (1.0f / segmentLength);
            const Vec3 bodyForward = bodyForwardFromAim(forward);
            const Vec3 bodyRight = normalized({bodyForward.z, 0.0f, -bodyForward.x});
            constexpr Vec3 worldUp{0.0f, 1.0f, 0.0f};
            const std::array<Vec3, 7> offsets{{
                {},
                bodyRight * fistRadius,
                bodyRight * -fistRadius,
                worldUp * fistRadius,
                worldUp * -fistRadius,
                right * (fistRadius * 0.62f) + up * (fistRadius * 0.62f),
                right * (fistRadius * -0.62f) + up * (fistRadius * -0.62f),
            }};
            for (const Vec3 offset : offsets) {
                const Vec3 origin = previousHand_ + offset;
                const auto hit = world.raycast(origin.x, origin.y, origin.z,
                                               direction.x, direction.y, direction.z,
                                               segmentLength + fistRadius * 0.72f);
                if (!hit.hit) continue;
                contactMade_ = true;
                pose_.contactMade = true;
                contact = SwingContact{hit};
                break;
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
