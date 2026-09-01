#pragma once

#include "world/chunks/ChunkCoord.h"

#include <cstdint>
#include <vector>

namespace rf::world {

enum class ChunkState : std::uint8_t {
    Ready,
    Dirty,
};

struct ChunkStreamDelta {
    std::vector<ChunkCoord> loaded;
    std::vector<ChunkCoord> unloaded;

    [[nodiscard]] bool changed() const noexcept {
        return !loaded.empty() || !unloaded.empty();
    }
};

} // namespace rf::world
