#include "save/FrontierSave.h"

#include "save/world/RegionFile.h"

#include <fstream>
#include <iomanip>
#include <string>

namespace rf::save {
namespace {
constexpr const char* signature = "RUNEFORGE_FRONTIER_SAVE";
constexpr int currentSchemaVersion = 2;

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

bool readLegacyEdits(std::istream& input, FrontierSaveData& data, std::size_t count) {
    data.edits.reserve(data.edits.size() + count);
    for (std::size_t i = 0; i < count; ++i) {
        world::BlockEdit edit;
        int block = 0;
        input >> edit.position.x >> edit.position.y >> edit.position.z >> block;
        if (!input || block < static_cast<int>(world::BlockId::Air) ||
            block > static_cast<int>(world::BlockId::Leaves)) return false;
        edit.block = static_cast<world::BlockId>(block);
        data.edits.push_back(edit);
    }
    return true;
}
} // namespace

bool frontierSaveExists(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

std::optional<FrontierSaveData> loadFrontierSave(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return std::nullopt;

    std::string header;
    int schema = 0;
    input >> header >> schema;
    if (header != signature || schema < 1 || schema > currentSchemaVersion) return std::nullopt;

    FrontierSaveData data;
    std::string token;
    while (input >> token) {
        if (token == "seed") {
            input >> data.seed;
        } else if (token == "player") {
            input >> data.playerPosition.x >> data.playerPosition.y >> data.playerPosition.z >> data.yaw >> data.pitch;
        } else if (token == "edits") {
            std::size_t count = 0;
            input >> count;
            if (!readLegacyEdits(input, data, count)) return std::nullopt;
        } else if (token == "region_store") {
            int enabled = 0;
            input >> enabled;
            if (enabled != 0) {
                auto regionEdits = worldstore::RegionFile::readAll(path.parent_path() / "regions");
                data.edits.insert(data.edits.end(), regionEdits.begin(), regionEdits.end());
            }
        }
        if (!input) return std::nullopt;
    }
    return data;
}

bool saveFrontierSave(const std::filesystem::path& path, const FrontierSaveData& data) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;

    const auto regionDirectory = path.parent_path() / "regions";
    if (!worldstore::RegionFile::writeAll(regionDirectory, data.edits)) return false;

    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;

    output << signature << ' ' << currentSchemaVersion << '\n';
    output << "seed " << data.seed << '\n';
    output << std::setprecision(9) << "player "
           << data.playerPosition.x << ' ' << data.playerPosition.y << ' ' << data.playerPosition.z << ' '
           << data.yaw << ' ' << data.pitch << '\n';
    output << "region_store 1\n";
    output.flush();
    if (!output) return false;
    output.close();
    return replaceFile(temporary, path);
}

} // namespace rf::save
