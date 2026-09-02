#pragma once

#include "world/WorldEdit.h"
#include "world/chunks/ChunkCoord.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>

namespace rf::world {
class FrontierWorld;
}

namespace rf::world::fluid {

// Lightweight authoritative fluid foundation. Stable generated water costs zero simulation work.
// Only water near edits/flow enters the active queue, and every tick has a strict cell budget.
class WaterSimulation {
public:
    static constexpr std::uint8_t fullLevel = 8;
    static constexpr float tickSeconds = 0.12f;
    static constexpr std::size_t maxCellsPerTick = 128;

    void reset() noexcept;
    void onExternalBlockChange(BlockCoord position, BlockId before, BlockId after) noexcept;
    [[nodiscard]] bool update(float deltaSeconds, FrontierWorld& world);
    void trim(ChunkCoord center, int retainRadius);

    [[nodiscard]] std::uint8_t levelAt(const FrontierWorld& world, BlockCoord position) const noexcept;
    [[nodiscard]] std::size_t activeCount() const noexcept { return active_.size(); }
    [[nodiscard]] std::size_t dynamicCellCount() const noexcept { return levels_.size(); }

private:
    void enqueue(BlockCoord position) noexcept;
    void enqueueNeighborhood(BlockCoord position) noexcept;
    void setLevel(FrontierWorld& world, BlockCoord position, std::uint8_t level);
    [[nodiscard]] bool updateCell(FrontierWorld& world, BlockCoord position);

    float accumulator_{};
    std::deque<BlockCoord> active_;
    std::set<BlockCoord> queued_;
    // Only partial cells need explicit storage. A world Water block absent from this map is full (8/8).
    std::map<BlockCoord, std::uint8_t> levels_;
};

} // namespace rf::world::fluid
