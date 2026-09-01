#pragma once

#include <cstdint>
#include <string_view>

namespace rf::game::mining {

enum class MiningMode : std::uint8_t {
    Block = 0,
    Micro,
    Mixed,
};

[[nodiscard]] constexpr std::string_view miningModeName(MiningMode mode) noexcept {
    switch (mode) {
        case MiningMode::Block: return "Block";
        case MiningMode::Micro: return "Micro";
        case MiningMode::Mixed: return "Mixed";
    }
    return "Mixed";
}

[[nodiscard]] constexpr MiningMode nextMiningMode(MiningMode mode) noexcept {
    switch (mode) {
        case MiningMode::Block: return MiningMode::Micro;
        case MiningMode::Micro: return MiningMode::Mixed;
        case MiningMode::Mixed: return MiningMode::Block;
    }
    return MiningMode::Mixed;
}

} // namespace rf::game::mining
