#include "game/interaction/MiningSwing.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>

namespace rf::game::interaction {
namespace {

constexpr float pi = 3.14159265358979323846f;
constexpr float impactTime = 0.30f;

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
        if (targetDistance_ <= interactionReach) targetPoint_ = contact;
        else {
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

    // Match the first-person viewmodel's compact Minecraft-like timing. The arm starts from its
    // existing anatomical rest pose, sweeps inward/forward quickly, and recovers without a long
    // below-camera windup. The desired point can move farther for distant targets, but the fixed
    // two-bone solve still clamps anatomy rather than telescoping the limb.
    const float normalizedT = std::clamp(t, 0.0f, 1.0f);
    const float root = std::sqrt(normalizedT);
    const float arc = std::sin(root * pi);
    const float forwardPulse = std::sin(normalizedT * pi);
    const float verticalWave = std::sin(root * pi * 2.0f);
    const float centerWeight = std::clamp(arc * arc * 0.82f, 0.0f, 0.82f);
    const float targetRatio = std::clamp(targetDistance_ / interactionReach, 0.0f, 1.0f);
    const float strikeDepth = 0.50f + targetRatio * 0.15f;
    const Vec3 centerStrike = eye + forward * strikeDepth;

    Vec3 desired = rest * (1.0f - centerWeight) + centerStrike * centerWeight;
    desired = desired - right * (arc * 0.070f)
                      + up * (verticalWave * 0.040f)
                      + forward * (forwardPulse * (0.025f + targetRatio * 0.025f));
    return desired;
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
    // Center-ray nomination remains authoritative. The impact fires at the early peak of the compact
    // viewmodel sweep, rather than the old 56%-through punch, so what the player sees and what the
    // terrain receives happen together. One swing still receives only one nominated solid contact.
    const bool crossedImpact = previousT < impactTime && t >= impactTime;
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
