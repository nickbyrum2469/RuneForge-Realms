#pragma once

#include "world/Block.h"

#include <array>
#include <cstddef>

namespace rf::world {

class VoxelChunk {
public:
    static constexpr int sizeX = 16;
    static constexpr int sizeY = 16;
    static constexpr int sizeZ = 16;
    static constexpr std::size_t blockCount = static_cast<std::size_t>(sizeX * sizeY * sizeZ);

    VoxelChunk();

    [[nodiscard]] BlockId get(int x, int y, int z) const noexcept;
    void set(int x, int y, int z, BlockId block) noexcept;
    void fill(BlockId block) noexcept;
    [[nodiscard]] std::size_t solidBlockCount() const noexcept;

private:
    [[nodiscard]] static constexpr bool inBounds(int x, int y, int z) noexcept {
        return x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ;
    }

    [[nodiscard]] static constexpr std::size_t index(int x, int y, int z) noexcept {
        return static_cast<std::size_t>(x + sizeX * (z + sizeZ * y));
    }

    std::array<BlockId, blockCount> blocks_{};
};

} // namespace rf::world
