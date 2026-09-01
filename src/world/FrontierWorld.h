#pragma once

#include "world/GreedyMesher.h"

#include <compare>
#include <cstdint>
#include <map>
#include <vector>

namespace rf::world {

struct ChunkCoord {
    int x{};
    int z{};
    auto operator<=>(const ChunkCoord&) const = default;
};

struct BlockCoord {
    int x{};
    int y{};
    int z{};
    auto operator<=>(const BlockCoord&) const = default;
};

struct BlockEdit {
    BlockCoord position{};
    BlockId block{BlockId::Air};
};

struct RaycastHit {
    bool hit{false};
    BlockCoord block{};
    BlockCoord adjacent{};
};

class FrontierWorld {
public:
    static constexpr int chunkRadius = 3;
    static constexpr int chunkDiameter = chunkRadius * 2 + 1;
    static constexpr int worldMin = -chunkRadius * VoxelChunk::sizeX;
    static constexpr int worldMax = (chunkRadius + 1) * VoxelChunk::sizeX - 1;

    void generate(std::uint32_t seed);

    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }
    [[nodiscard]] BlockId getBlock(int x, int y, int z) const noexcept;
    bool setBlock(int x, int y, int z, BlockId block, bool recordEdit = true);
    [[nodiscard]] int topSolidY(int x, int z) const noexcept;
    [[nodiscard]] bool collidesAabb(float minX, float minY, float minZ,
                                    float maxX, float maxY, float maxZ) const noexcept;
    [[nodiscard]] RaycastHit raycast(float ox, float oy, float oz,
                                     float dx, float dy, float dz,
                                     float maxDistance) const noexcept;
    [[nodiscard]] VoxelMesh buildMesh() const;
    [[nodiscard]] std::size_t solidBlockCount() const noexcept;

    [[nodiscard]] std::vector<BlockEdit> edits() const;
    void applyEdit(const BlockEdit& edit);
    void clearEdits() noexcept { edits_.clear(); }

private:
    [[nodiscard]] static int floorDiv(int value, int divisor) noexcept;
    [[nodiscard]] static int floorMod(int value, int divisor) noexcept;
    [[nodiscard]] VoxelChunk* chunkAt(int chunkX, int chunkZ) noexcept;
    [[nodiscard]] const VoxelChunk* chunkAt(int chunkX, int chunkZ) const noexcept;
    void generateTerrain();
    void generateTrees();

    std::uint32_t seed_{1337};
    std::map<ChunkCoord, VoxelChunk> chunks_;
    std::map<BlockCoord, BlockId> edits_;
};

} // namespace rf::world
