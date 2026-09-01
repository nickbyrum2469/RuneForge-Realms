#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rf::world {

enum class BlockId : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Wood,
    Leaves,
};

enum class SurfaceMaterial : std::uint32_t {
    Grass = 0,
    Dirt = 1,
    Stone = 2,
    Wood = 3,
    Leaves = 4,
};

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
    [[nodiscard]] static VoxelChunk makeDemoTerrain();

private:
    [[nodiscard]] static constexpr bool inBounds(int x, int y, int z) noexcept {
        return x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ;
    }

    [[nodiscard]] static constexpr std::size_t index(int x, int y, int z) noexcept {
        return static_cast<std::size_t>(x + sizeX * (z + sizeZ * y));
    }

    std::array<BlockId, blockCount> blocks_{};
};

[[nodiscard]] constexpr bool isSolid(BlockId block) noexcept {
    return block != BlockId::Air;
}

[[nodiscard]] SurfaceMaterial surfaceMaterial(BlockId block, int axis, int normalSign) noexcept;

} // namespace rf::world
