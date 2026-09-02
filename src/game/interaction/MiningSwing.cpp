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
    right = normalized(right);
    if (lengthSquared(right) <= 0.000001f) right = {1.0f, 0.0f, 0.0f};
    up = normalized(cross(forward, right));
    if (lengthSquared(up) <= 0.000001f) up = {0.0f, 1.0f, 0.0f};
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

bool MiningSwing::begin(const world::RaycastHit& target, Vec3 eye, Vec3 forward, Vec3 right, Vec3 up,
                        float durationSeconds) noexcept {
    if (active_ || !target.hit) return false;
    const Vec3 contact{target.worldX, target.worldY, target.worldZ};
    if (distance(eye, contact) > fistReach) return false;
    canonicalizeBasis(forward, right, up);

    active_ = true;
    contactMade_ = false;
    elapsed_ = 0.0f;
    duration_ = std::clamp(durationSeconds, 0.34f, 1.20f);
    target_ = target;
    targetPoint_ = contact;
    previousHand_ = calculateHand(0.0f, eye, forward, right, up);
    updatePose(0.0f, eye, forward, right, up);
    return true;
}

Vec3 MiningSwing::calculateHand(float t, Vec3 eye, Vec3 forward, Vec3 right, Vec3 up) const noexcept {
    const Vec3 rest = eye + forward * 0.34f + right * 0.31f - up * 0.35f;
    const Vec3 windup = rest - forward * 0.20f + right * 0.12f - up * 0.04f;
    const Vec3 strike = targetPoint_ + forward * 0.075f;

    if (t < 0.28f) {
        const float u = smooth01(t / 0.28f);
        return rest * (1.0f - u) + windup * u;
    }
    if (t < 0.62f) {
        const float u = smooth01((t - 0.28f) / 0.34f);
        // A small down-and-in arc keeps the fist from feeling like a camera-space piston.
        const Vec3 arc = up * (std::sin(u * 3.14159265f) * 0.055f) - right * (std::sin(u * 3.14159265f) * 0.035f);
        return windup * (1.0f - u) + strike * u + arc;
    }
    const float u = smooth01((t - 0.62f) / 0.38f);
    const Vec3 follow = strike + forward * 0.035f - up * 0.025f;
    return follow * (1.0f - u) + rest * u;
}

void MiningSwing::updatePose(float t, Vec3 eye, Vec3 forward, Vec3 right, Vec3 up) noexcept {
    pose_.active = active_;
    pose_.contactMade = contactMade_;
    pose_.normalizedTime = std::clamp(t, 0.0f, 1.0f);
    pose_.shoulder = eye + right * 0.31f - up * 0.29f - forward * 0.02f;
    pose_.hand = calculateHand(t, eye, forward, right, up);
    const Vec3 midpoint = (pose_.shoulder + pose_.hand) * 0.5f;
    // Elbow hangs slightly low/outboard so the visible arm forms an actual bent limb.
    pose_.elbow = midpoint - up * 0.10f + right * 0.055f;
}

std::optional<SwingContact> MiningSwing::update(float deltaSeconds, const world::FrontierWorld& world,
                                                Vec3 eye, Vec3 forward, Vec3 right, Vec3 up) noexcept {
    if (!active_) return std::nullopt;
    canonicalizeBasis(forward, right, up);

    const float previousT = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    elapsed_ += std::clamp(deltaSeconds, 0.0f, 0.08f);
    const float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    const Vec3 hand = calculateHand(t, eye, forward, right, up);
    updatePose(t, eye, forward, right, up);

    std::optional<SwingContact> contact;
    const bool strikeWindow = t >= 0.25f && previousT <= 0.68f;
    if (!contactMade_ && strikeWindow) {
        const Vec3 segment = hand - previousHand_;
        const float segmentLength = std::sqrt(lengthSquared(segment));
        if (segmentLength > 0.0005f) {
            const Vec3 direction = segment * (1.0f / segmentLength);
            const std::array<Vec3, 5> offsets{{
                {}, right * fistRadius, right * -fistRadius, up * fistRadius, up * -fistRadius,
            }};
            for (const Vec3 offset : offsets) {
                const Vec3 origin = previousHand_ + offset;
                const auto hit = world.raycast(origin.x, origin.y, origin.z,
                                               direction.x, direction.y, direction.z,
                                               segmentLength + fistRadius * 0.65f);
                if (!hit.hit || hit.block != target_.block) continue;
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