#include "game/PlayerController.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>

namespace rf::game {

void PlayerController::spawn(Vec3 feetPosition, float yaw, float pitch) noexcept {
    position_ = feetPosition;
    velocity_ = {};
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -1.45f, 1.45f);
    grounded_ = false;
    jumpRequested_ = false;
}

void PlayerController::setMouseSensitivity(float scale) noexcept {
    mouseSensitivity_ = 0.00215f * std::clamp(scale, 0.25f, 2.50f);
}

void PlayerController::setControl(MoveControl control, bool pressed) noexcept {
    switch (control) {
        case MoveControl::Forward: forward_ = pressed; break;
        case MoveControl::Backward: backward_ = pressed; break;
        case MoveControl::Left: left_ = pressed; break;
        case MoveControl::Right: right_ = pressed; break;
        case MoveControl::Sprint: sprint_ = pressed; break;
        case MoveControl::Crouch: crouch_ = pressed; break;
    }
}

void PlayerController::addLook(float deltaX, float deltaY) noexcept {
    yaw_ += deltaX * mouseSensitivity_;
    pitch_ = std::clamp(pitch_ - deltaY * mouseSensitivity_, -1.45f, 1.45f);
}

bool PlayerController::collides(const world::FrontierWorld& world, Vec3 position) const noexcept {
    const float halfWidth = bodyWidth() * 0.5f;
    return world.collidesAabb(position.x - halfWidth, position.y, position.z - halfWidth,
                              position.x + halfWidth, position.y + bodyHeight(), position.z + halfWidth);
}

void PlayerController::moveAxis(const world::FrontierWorld& world, float amount, int axis) noexcept {
    if (std::abs(amount) < 0.000001f) return;
    Vec3 candidate = position_;
    if (axis == 0) candidate.x += amount;
    else if (axis == 1) candidate.y += amount;
    else candidate.z += amount;

    if (!collides(world, candidate)) {
        position_ = candidate;
        return;
    }

    if (axis == 0) velocity_.x = 0.0f;
    else if (axis == 1) velocity_.y = 0.0f;
    else velocity_.z = 0.0f;
}

void PlayerController::update(float deltaSeconds, const world::FrontierWorld& world) noexcept {
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);
    Vec3 wish{};
    if (forward_) wish += horizontalForward(yaw_);
    if (backward_) wish -= horizontalForward(yaw_);
    if (right_) wish += horizontalRight(yaw_);
    if (left_) wish -= horizontalRight(yaw_);
    wish = normalized(wish);

    const float speed = crouch_ ? 2.5f : (sprint_ ? 7.4f : 4.8f);
    velocity_.x = wish.x * speed;
    velocity_.z = wish.z * speed;

    const float halfWidth = bodyWidth() * 0.5f;
    grounded_ = world.collidesAabb(position_.x - halfWidth, position_.y - 0.055f, position_.z - halfWidth,
                                    position_.x + halfWidth, position_.y, position_.z + halfWidth);
    if (jumpRequested_ && grounded_ && !crouch_) {
        velocity_.y = 6.25f;
        grounded_ = false;
    }
    jumpRequested_ = false;

    velocity_.y -= 18.5f * dt;
    velocity_.y = std::max(velocity_.y, -28.0f);

    moveAxis(world, velocity_.x * dt, 0);
    moveAxis(world, velocity_.z * dt, 2);
    const float beforeY = position_.y;
    moveAxis(world, velocity_.y * dt, 1);
    if (velocity_.y <= 0.0f && position_.y == beforeY && collides(world, {position_.x, position_.y - 0.02f, position_.z})) {
        grounded_ = true;
    }
}

} // namespace rf::game
