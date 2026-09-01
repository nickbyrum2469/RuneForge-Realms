#pragma once

#include "world/VoxelChunk.h"
#include "world/chunks/ChunkState.h"

#include <cstddef>
#include <functional>
#include <map>
#include <vector>

namespace rf::world {

class ChunkManager {
public:
    using Generator = std::function<VoxelChunk(ChunkCoord)>;

    struct Record {
        VoxelChunk voxels;
        ChunkState state{ChunkState::Ready};
    };

    void clear() noexcept;

    [[nodiscard]] ChunkStreamDelta update(ChunkCoord center,
                                          int loadRadius,
                                          int retainRadius,
                                          const Generator& generator);

    [[nodiscard]] VoxelChunk* find(ChunkCoord coord) noexcept;
    [[nodiscard]] const VoxelChunk* find(ChunkCoord coord) const noexcept;
    [[nodiscard]] bool contains(ChunkCoord coord) const noexcept;
    [[nodiscard]] std::size_t loadedCount() const noexcept { return chunks_.size(); }
    [[nodiscard]] std::vector<ChunkCoord> loadedCoords() const;
    [[nodiscard]] std::vector<ChunkCoord> dirtyCoords() const;

    void markDirty(ChunkCoord coord) noexcept;
    void markReady(ChunkCoord coord) noexcept;

private:
    std::map<ChunkCoord, Record> chunks_;
};

} // namespace rf::world
