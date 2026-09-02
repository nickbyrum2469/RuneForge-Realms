#pragma once

#include <algorithm>
#include <cmath>

namespace rf::game {

struct Vec3 {
    float x{};
    float y{};
    float z{};

    Vec3& operator+=(const Vec3& other) noexcept {
        x += other.x; y += other.y; z += other.z; return *this;
    }
    Vec3& operator-=(const Vec3& other) noexcept {
        x -= other.x; y -= other.y; z -= other.z; return *this;
    }
};

[[nodiscard]] inline Vec3 operator+(Vec3 a, const Vec3& b) noexcept { return a += b; }
[[nodiscard]] inline Vec3 operator-(Vec3 a, const Vec3& b) noexcept { return a -= b; }
[[nodiscard]] inline Vec3 operator*(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline float dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

[[nodiscard]] inline float lengthSquared(const Vec3& value) noexcept {
    return dot(value, value);
}

[[nodiscard]] inline Vec3 normalized(Vec3 value) noexcept {
    const float lenSq = lengthSquared(value);
    if (lenSq <= 0.000001f) return {};
    const float inv = 1.0f / std::sqrt(lenSq);
    return value * inv;
}

[[nodiscard]] inline Vec3 forwardFromAngles(float yaw, float pitch) noexcept {
    const float cp = std::cos(pitch);
    return normalized({std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp});
}

[[nodiscard]] inline Vec3 horizontalForward(float yaw) noexcept {
    return {std::sin(yaw), 0.0f, std::cos(yaw)};
}

[[nodiscard]] inline Vec3 horizontalRight(float yaw) noexcept {
    return {std::cos(yaw), 0.0f, -std::sin(yaw)};
}

} // namespace rf::game