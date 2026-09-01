#pragma once

#include "save/world/RegionFile.h"
#include "world/micro/MicroVoxelEdit.h"

#include <filesystem>
#include <vector>

namespace rf::save::worldstore {

class MicroRegionFile {
public:
    [[nodiscard]] static std::filesystem::path pathFor(const std::filesystem::path& directory,
                                                       RegionCoord coord);
    static bool writeAll(const std::filesystem::path& directory,
                         const std::vector<rf::world::micro::MicroVoxelEdit>& edits);
    [[nodiscard]] static std::vector<rf::world::micro::MicroVoxelEdit>
    readAll(const std::filesystem::path& directory);

private:
    static bool writeOne(const std::filesystem::path& path, RegionCoord coord,
                         const std::vector<rf::world::micro::MicroVoxelEdit>& edits);
    [[nodiscard]] static std::vector<rf::world::micro::MicroVoxelEdit>
    readOne(const std::filesystem::path& path);
};

} // namespace rf::save::worldstore
