#pragma once

#include "world/WorldEdit.h"

#include <compare>
#include <filesystem>
#include <vector>

namespace rf::save::worldstore {

struct RegionCoord {
    int x{};
    int z{};
    auto operator<=>(const RegionCoord&) const = default;
};

class RegionFile {
public:
    static constexpr int chunksPerRegion = 32;
    static constexpr int blocksPerRegion = chunksPerRegion * 16;

    [[nodiscard]] static RegionCoord regionForBlock(int blockX, int blockZ) noexcept;
    [[nodiscard]] static std::filesystem::path pathFor(const std::filesystem::path& directory, RegionCoord coord);

    static bool writeAll(const std::filesystem::path& directory, const std::vector<rf::world::BlockEdit>& edits);
    [[nodiscard]] static std::vector<rf::world::BlockEdit> readAll(const std::filesystem::path& directory);

private:
    static bool writeOne(const std::filesystem::path& path, RegionCoord coord,
                         const std::vector<rf::world::BlockEdit>& edits);
    [[nodiscard]] static std::vector<rf::world::BlockEdit> readOne(const std::filesystem::path& path);
};

} // namespace rf::save::worldstore
