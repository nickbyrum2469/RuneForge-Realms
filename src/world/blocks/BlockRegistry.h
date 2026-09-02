#pragma once

#include "world/blocks/BlockDefinition.h"

#include <array>
#include <cstddef>

namespace rf::world::blocks {

class BlockRegistry {
public:
    static constexpr std::size_t blockCount = 7;

    [[nodiscard]] static const BlockDefinition& get(BlockId id) noexcept;
    [[nodiscard]] static const std::array<BlockDefinition, blockCount>& all() noexcept;
};

} // namespace rf::world::blocks
