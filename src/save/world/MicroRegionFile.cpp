#include "save/world/MicroRegionFile.h"

#include "save/TransactionalFileReplace.h"

#include <fstream>
#include <map>
#include <set>
#include <string>

namespace rf::save::worldstore {
namespace {
constexpr const char* signature = "RUNEFORGE_MICRO_REGION";
constexpr int schemaVersion = 1;
} // namespace

std::filesystem::path MicroRegionFile::pathFor(const std::filesystem::path& directory, RegionCoord coord) {
    return directory / ("m." + std::to_string(coord.x) + "." + std::to_string(coord.z) + ".rfm");
}

bool MicroRegionFile::writeOne(const std::filesystem::path& path, RegionCoord coord,
                               const std::vector<rf::world::micro::MicroVoxelEdit>& edits) {
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << signature << ' ' << schemaVersion << ' ' << coord.x << ' ' << coord.z << '\n';
    output << "micro_edits " << edits.size() << '\n';
    for (const auto& edit : edits) {
        output << edit.position.x << ' ' << edit.position.y << ' ' << edit.position.z << ' '
               << static_cast<int>(edit.block);
        for (const std::uint64_t word : edit.occupancyWords) output << ' ' << word;
        output << '\n';
    }
    output.flush();
    if (!output) return false;
    output.close();
    return rf::save::detail::replaceFileRollbackSafe(temporary, path);
}

bool MicroRegionFile::writeAll(const std::filesystem::path& directory,
                               const std::vector<rf::world::micro::MicroVoxelEdit>& edits) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return false;

    std::map<RegionCoord, std::vector<rf::world::micro::MicroVoxelEdit>> grouped;
    for (const auto& edit : edits) {
        grouped[RegionFile::regionForBlock(edit.position.x, edit.position.z)].push_back(edit);
    }

    std::set<std::filesystem::path> expected;
    for (const auto& [coord, regionEdits] : grouped) {
        const auto path = pathFor(directory, coord);
        if (!writeOne(path, coord, regionEdits)) return false;
        expected.insert(path.filename());
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) return false;
        if (!entry.is_regular_file() || entry.path().extension() != ".rfm") continue;
        if (expected.contains(entry.path().filename())) continue;
        std::filesystem::remove(entry.path(), ec);
        if (ec) return false;
    }
    return true;
}

std::vector<rf::world::micro::MicroVoxelEdit>
MicroRegionFile::readOne(const std::filesystem::path& path) {
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
    if (!input || token != "micro_edits") return {};

    std::vector<rf::world::micro::MicroVoxelEdit> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        rf::world::micro::MicroVoxelEdit edit;
        int block = 0;
        input >> edit.position.x >> edit.position.y >> edit.position.z >> block;
        if (!input || block <= static_cast<int>(rf::world::BlockId::Air) ||
            block > static_cast<int>(rf::world::BlockId::Leaves)) return {};
        if (RegionFile::regionForBlock(edit.position.x, edit.position.z) != coord) return {};
        edit.block = static_cast<rf::world::BlockId>(block);
        for (auto& word : edit.occupancyWords) input >> word;
        if (!input) return {};
        result.push_back(edit);
    }
    return result;
}

std::vector<rf::world::micro::MicroVoxelEdit>
MicroRegionFile::readAll(const std::filesystem::path& directory) {
    std::vector<rf::world::micro::MicroVoxelEdit> result;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".rfm") continue;
        auto edits = readOne(entry.path());
        result.insert(result.end(), edits.begin(), edits.end());
    }
    return result;
}

} // namespace rf::save::worldstore
