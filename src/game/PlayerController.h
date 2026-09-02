#pragma once

#include "game/Math.h"

#include <cmath>

namespace rf::world { class FrontierWorld; }

namespace rf::game {

enum class MoveControl { Forward, Backward, Left, Right, Sprint, Crouch };
enum class CameraMode { FirstPerson, ThirdPerson };

class PlayerController {
public:
    void spawn(Vec3 feetPosition, float yaw = 0.0f, float pitch = -0.08f) noexcept;
    void setControl(MoveControl control, bool pressed) noexcept;
    void requestJump() noexcept { jumpRequested_ = true; }
    void addLook(float deltaX, float deltaY) noexcept;
    void update(float deltaSeconds, const world::FrontierWorld& world) noexcept;
    void setMouseSensitivity(float scale) noexcept;
    void toggleCameraMode() noexcept {
        cameraMode_ = cameraMode_ == CameraMode::FirstPerson ? CameraMode::ThirdPerson : CameraMode::FirstPerson;
    }

    [[nodiscard]] Vec3 position() const noexcept { return position_; }
    [[nodiscard]] Vec3 eyePosition() const noexcept { return {position_.x, position_.y + eyeHeight(), position_.z}; }
    [[nodiscard]] Vec3 lookDirection() const noexcept { return forwardFromAngles(yaw_, pitch_); }
    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }
    [[nodiscard]] bool grounded() const noexcept { return grounded_; }
    [[nodiscard]] bool crouching() const noexcept { return crouch_; }
    [[nodiscard]] CameraMode cameraMode() const noexcept { return cameraMode_; }
    [[nodiscard]] bool thirdPerson() const noexcept { return cameraMode_ == CameraMode::ThirdPerson; }
    [[nodiscard]] float horizontalSpeed() const noexcept {
        return std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    }
    [[nodiscard]] float bodyWidth() const noexcept { return crouch_ ? 0.58f : 0.62f; }
    [[nodiscard]] float bodyHeight() const noexcept { return crouch_ ? 1.25f : 1.80f; }
    [[nodiscard]] float eyeHeight() const noexcept { return crouch_ ? 1.10f : 1.62f; }

private:
    [[nodiscard]] bool collides(const world::FrontierWorld& world, Vec3 position) const noexcept;
    void moveAxis(const world::FrontierWorld& world, float amount, int axis) noexcept;

    Vec3 position_{0.5f, 10.0f, 0.5f};
    Vec3 velocity_{};
    float yaw_{};
    float pitch_{-0.08f};
    float mouseSensitivity_{0.00215f};
    CameraMode cameraMode_{CameraMode::FirstPerson};
    bool forward_{};
    bool backward_{};
    bool left_{};
    bool right_{};
    bool sprint_{};
    bool crouch_{};
    bool jumpRequested_{};
    bool grounded_{};
};

} // namespace rf::game
