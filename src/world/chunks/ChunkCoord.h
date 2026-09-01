#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>

namespace rf::world {

struct ChunkCoord {
    int x{};
    int z{};
    auto operator<=>(const ChunkCoord&) const = default;
};

struct ChunkCoordHash {
    [[nodiscard]] std::size_t operator()(const ChunkCoord& value) const noexcept {
        const auto x = static_cast<std::uint32_t>(value.x);
        const auto z = static_cast<std::uint32_t>(value.z);
        std::uint64_t mixed = (static_cast<std::uint64_t>(x) << 32) | z;
        mixed ^= mixed >> 33;
        mixed *= 0xff51afd7ed558ccdULL;
        mixed ^= mixed >> 33;
        return static_cast<std::size_t>(mixed);
    }
};

[[nodiscard]] constexpr int floorDiv(int value, int divisor) noexcept {
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) --quotient;
    return quotient;
}

[[nodiscard]] constexpr int floorMod(int value, int divisor) noexcept {
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

} // namespace rf::world
