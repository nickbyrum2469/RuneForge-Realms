#include "save/world/RegionFile.h"

#include "world/chunks/ChunkCoord.h"

#include <fstream>
#include <map>
#include <set>
#include <string>

namespace rf::save::worldstore {
namespace {
constexpr const char* signature = "RUNEFORGE_REGION";
constexpr int schemaVersion = 1;

bool replaceFile(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
    std::error_code ec;
    std::filesystem::rename(temporary, destination, ec);
    if (!ec) return true;
    ec.clear();
    std::filesystem::remove(destination, ec);
    ec.clear();
    std::filesystem::rename(temporary, destination, ec);
    return !ec;
}
} // namespace

RegionCoord RegionFile::regionForBlock(int blockX, int blockZ) noexcept {
    return {rf::world::floorDiv(blockX, blocksPerRegion), rf::world::floorDiv(blockZ, blocksPerRegion)};
}

std::filesystem::path RegionFile::pathFor(const std::filesystem::path& directory, RegionCoord coord) {
    return directory / ("r." + std::to_string(coord.x) + "." + std::to_string(coord.z) + ".rfr");
}

bool RegionFile::writeOne(const std::filesystem::path& path, RegionCoord coord,
                          const std::vector<rf::world::BlockEdit>& edits) {
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << signature << ' ' << schemaVersion << ' ' << coord.x << ' ' << coord.z << '\n';
    output << "edits " << edits.size() << '\n';
    for (const auto& edit : edits) {
        output << edit.position.x << ' ' << edit.position.y << ' ' << edit.position.z << ' '
               << static_cast<int>(edit.block) << '\n';
    }
    output.flush();
    if (!output) return false;
    output.close();
    return replaceFile(temporary, path);
}

bool RegionFile::writeAll(const std::filesystem::path& directory,
                          const std::vector<rf::world::BlockEdit>& edits) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return false;

    std::map<RegionCoord, std::vector<rf::world::BlockEdit>> grouped;
    for (const auto& edit : edits) grouped[regionForBlock(edit.position.x, edit.position.z)].push_back(edit);

    std::set<std::filesystem::path> expected;
    for (const auto& [coord, regionEdits] : grouped) {
        const auto path = pathFor(directory, coord);
        if (!writeOne(path, coord, regionEdits)) return false;
        expected.insert(path.filename());
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) return false;
        if (!entry.is_regular_file() || entry.path().extension() != ".rfr") continue;
        if (expected.contains(entry.path().filename())) continue;
        std::filesystem::remove(entry.path(), ec);
        if (ec) return false;
    }
    return true;
}

std::vector<rf::world::BlockEdit> RegionFile::readOne(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return {};
    std::string header;
    int schema = 0;
    RegionCoord coord{};
    input >> header >> schema >> coord.x >> coord.z;
    if (!input || header != signature || schema != schemaVersion) return {};

    std::string token;
    std::size_t count = 0;
    input >> token >> count;
    if (!input || token != "edits") return {};

    std::vector<rf::world::BlockEdit> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        rf::world::BlockEdit edit;
        int block = 0;
        input >> edit.position.x >> edit.position.y >> edit.position.z >> block;
        if (!input || block < static_cast<int>(rf::world::BlockId::Air) ||
            block > static_cast<int>(rf::world::BlockId::Leaves)) return {};
        if (regionForBlock(edit.position.x, edit.position.z) != coord) return {};
        edit.block = static_cast<rf::world::BlockId>(block);
        result.push_back(edit);
    }
    return result;
}

std::vector<rf::world::BlockEdit> RegionFile::readAll(const std::filesystem::path& directory) {
    std::vector<rf::world::BlockEdit> result;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".rfr") continue;
        auto edits = readOne(entry.path());
        result.insert(result.end(), edits.begin(), edits.end());
    }
    return result;
}

} // namespace rf::save::worldstore
