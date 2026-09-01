#include "save/FrontierSave.h"

#include <fstream>
#include <iomanip>
#include <string>

namespace rf::save {
namespace {
constexpr const char* signature = "RUNEFORGE_FRONTIER_SAVE";
constexpr int schemaVersion = 1;
}

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
    if (header != signature || schema != schemaVersion) return std::nullopt;

    FrontierSaveData data;
    std::string token;
    std::size_t editCount = 0;
    while (input >> token) {
        if (token == "seed") {
            input >> data.seed;
        } else if (token == "player") {
            input >> data.playerPosition.x >> data.playerPosition.y >> data.playerPosition.z >> data.yaw >> data.pitch;
        } else if (token == "edits") {
            input >> editCount;
            data.edits.reserve(editCount);
            for (std::size_t i = 0; i < editCount; ++i) {
                world::BlockEdit edit;
                int block = 0;
                input >> edit.position.x >> edit.position.y >> edit.position.z >> block;
                if (!input || block < static_cast<int>(world::BlockId::Air) || block > static_cast<int>(world::BlockId::Leaves)) {
                    return std::nullopt;
                }
                edit.block = static_cast<world::BlockId>(block);
                data.edits.push_back(edit);
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

    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;

    output << signature << ' ' << schemaVersion << '\n';
    output << "seed " << data.seed << '\n';
    output << std::setprecision(9) << "player "
           << data.playerPosition.x << ' ' << data.playerPosition.y << ' ' << data.playerPosition.z << ' '
           << data.yaw << ' ' << data.pitch << '\n';
    output << "edits " << data.edits.size() << '\n';
    for (const auto& edit : data.edits) {
        output << edit.position.x << ' ' << edit.position.y << ' ' << edit.position.z << ' '
               << static_cast<int>(edit.block) << '\n';
    }
    output.flush();
    if (!output) return false;
    output.close();

    std::filesystem::rename(temporary, path, ec);
    if (!ec) return true;

    // Windows rename does not replace an existing file. Keep the old file until the new one is complete,
    // then perform the small replacement window.
    ec.clear();
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
    return !ec;
}

} // namespace rf::save
