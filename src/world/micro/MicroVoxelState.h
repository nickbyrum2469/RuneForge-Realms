#pragma once

#include <array>
#include <bitset>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace rf::world::micro {

inline constexpr int resolution = 8;
inline constexpr int cellCount = resolution * resolution * resolution;
inline constexpr float cellSize = 1.0f / static_cast<float>(resolution);

struct MicroCoord {
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t z{};
    auto operator<=>(const MicroCoord&) const = default;
};

class MicroVoxelState {
public:
    MicroVoxelState();

    [[nodiscard]] bool occupied(int x, int y, int z) const noexcept;
    bool setOccupied(int x, int y, int z, bool value) noexcept;
    std::size_t clearSphere(MicroCoord center, int radiusCells) noexcept;

    [[nodiscard]] bool full() const noexcept { return occupied_.all(); }
    [[nodiscard]] bool empty() const noexcept { return occupied_.none(); }
    [[nodiscard]] std::size_t occupiedCount() const noexcept { return occupied_.count(); }
    [[nodiscard]] float solidFraction() const noexcept {
        return static_cast<float>(occupied_.count()) / static_cast<float>(cellCount);
    }

    [[nodiscard]] const std::bitset<cellCount>& bits() const noexcept { return occupied_; }
    void assignBits(const std::bitset<cellCount>& bits) noexcept { occupied_ = bits; }

    [[nodiscard]] static constexpr std::size_t index(int x, int y, int z) noexcept {
        return static_cast<std::size_t>(x + resolution * (z + resolution * y));
    }

private:
    std::bitset<cellCount> occupied_;
};

} // namespace rf::world::micro
