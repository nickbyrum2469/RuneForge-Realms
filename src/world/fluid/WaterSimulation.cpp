#include "world/fluid/WaterSimulation.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rf::world::fluid {
namespace {

constexpr std::array<BlockCoord, 6> kNeighbors{{
    {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1},
}};
constexpr std::array<BlockCoord, 4> kHorizontal{{
    {-1,0,0}, {1,0,0}, {0,0,-1}, {0,0,1},
}};

BlockCoord add(BlockCoord a, BlockCoord b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

} // namespace

void WaterSimulation::reset() noexcept {
    accumulator_ = 0.0f;
    active_.clear();
    queued_.clear();
    levels_.clear();
}

void WaterSimulation::enqueue(BlockCoord position) noexcept {
    if (position.y < 0 || position.y >= VoxelChunk::sizeY) return;
    if (!queued_.insert(position).second) return;
    active_.push_back(position);
}

void WaterSimulation::enqueueNeighborhood(BlockCoord position) noexcept {
    enqueue(position);
    for (const auto offset : kNeighbors) enqueue(add(position, offset));
}

void WaterSimulation::onExternalBlockChange(BlockCoord position, BlockId before, BlockId after) noexcept {
    if (after != BlockId::Water) levels_.erase(position);
    if (before == after) return;
    enqueueNeighborhood(position);
}

std::uint8_t WaterSimulation::levelAt(const FrontierWorld& world, BlockCoord position) const noexcept {
    if (position.y < 0 || position.y >= VoxelChunk::sizeY) return 0;
    const auto stored = levels_.find(position);
    if (stored != levels_.end()) return stored->second;
    return world.getBlock(position.x, position.y, position.z) == BlockId::Water ? fullLevel : 0;
}

void WaterSimulation::setLevel(FrontierWorld& world, BlockCoord position, std::uint8_t level) {
    level = std::min(level, fullLevel);
    if (level == 0) {
        levels_.erase(position);
        if (world.getBlock(position.x, position.y, position.z) == BlockId::Water) {
            (void)world.setBlock(position.x, position.y, position.z, BlockId::Air, false);
        }
        return;
    }

    if (world.getBlock(position.x, position.y, position.z) != BlockId::Water) {
        (void)world.setBlock(position.x, position.y, position.z, BlockId::Water, false);
    }
    if (level >= fullLevel) levels_.erase(position);
    else levels_[position] = level;
}

bool WaterSimulation::updateCell(FrontierWorld& world, BlockCoord position) {
    std::uint8_t sourceLevel = levelAt(world, position);
    if (sourceLevel == 0 || world.getBlock(position.x, position.y, position.z) != BlockId::Water) {
        levels_.erase(position);
        return false;
    }

    bool changed = false;

    // Gravity wins. A falling column can move an entire cell's volume in one scheduled update,
    // which makes waterfalls responsive without increasing the global simulation tick rate.
    if (position.y > 0) {
        const BlockCoord below{position.x, position.y - 1, position.z};
        const BlockId belowBlock = world.getBlock(below.x, below.y, below.z);
        if (belowBlock == BlockId::Air || belowBlock == BlockId::Water) {
            const std::uint8_t belowLevel = levelAt(world, below);
            if (belowLevel < fullLevel) {
                const std::uint8_t transfer = std::min<std::uint8_t>(sourceLevel, fullLevel - belowLevel);
                if (transfer > 0) {
                    sourceLevel = static_cast<std::uint8_t>(sourceLevel - transfer);
                    setLevel(world, position, sourceLevel);
                    setLevel(world, below, static_cast<std::uint8_t>(belowLevel + transfer));
                    enqueueNeighborhood(position);
                    enqueueNeighborhood(below);
                    return true;
                }
            }
        }
    }

    // When downward flow is blocked, equalize horizontally in small packets. Limiting each lateral
    // transfer keeps streams from teleporting across a basin in a single simulation tick.
    for (const auto offset : kHorizontal) {
        if (sourceLevel <= 1) break;
        const BlockCoord neighbor = add(position, offset);
        const BlockId neighborBlock = world.getBlock(neighbor.x, neighbor.y, neighbor.z);
        if (neighborBlock != BlockId::Air && neighborBlock != BlockId::Water) continue;
        const std::uint8_t neighborLevel = levelAt(world, neighbor);
        if (sourceLevel <= neighborLevel + 1) continue;

        const int difference = static_cast<int>(sourceLevel) - static_cast<int>(neighborLevel);
        const std::uint8_t transfer = static_cast<std::uint8_t>(std::clamp(difference / 2, 1, 2));
        sourceLevel = static_cast<std::uint8_t>(sourceLevel - transfer);
        setLevel(world, position, sourceLevel);
        setLevel(world, neighbor, static_cast<std::uint8_t>(neighborLevel + transfer));
        enqueueNeighborhood(neighbor);
        changed = true;
    }

    if (changed) enqueueNeighborhood(position);
    return changed;
}

bool WaterSimulation::update(float deltaSeconds, FrontierWorld& world) {
    accumulator_ += std::clamp(deltaSeconds, 0.0f, 0.25f);
    bool changed = false;
    int ticks = 0;
    while (accumulator_ >= tickSeconds && ticks < 2) {
        accumulator_ -= tickSeconds;
        ++ticks;
        const std::size_t work = std::min(maxCellsPerTick, active_.size());
        for (std::size_t i = 0; i < work; ++i) {
            const BlockCoord position = active_.front();
            active_.pop_front();
            queued_.erase(position);
            changed = updateCell(world, position) || changed;
        }
    }
    return changed;
}

void WaterSimulation::trim(ChunkCoord center, int retainRadius) {
    const auto retained = [center, retainRadius](BlockCoord position) {
        return chebyshevDistance(chunkFromBlock(position.x, position.z), center) <= retainRadius;
    };

    for (auto it = levels_.begin(); it != levels_.end();) {
        if (!retained(it->first)) it = levels_.erase(it);
        else ++it;
    }

    std::deque<BlockCoord> kept;
    queued_.clear();
    while (!active_.empty()) {
        const BlockCoord position = active_.front();
        active_.pop_front();
        if (!retained(position)) continue;
        if (!queued_.insert(position).second) continue;
        kept.push_back(position);
    }
    active_ = std::move(kept);
}

} // namespace rf::world::fluid
