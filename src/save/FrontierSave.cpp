#include "save/FrontierSave.h"

#include "save/world/MicroRegionFile.h"
#include "save/world/RegionFile.h"

#include <fstream>
#include <iomanip>
#include <string>

namespace rf::save {
namespace {
constexpr const char* signature = "RUNEFORGE_FRONTIER_SAVE";
constexpr int currentSchemaVersion = 3;

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

bool readInlineEdits(std::istream& input, FrontierSaveData& data, std::size_t count) {
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

bool readInventory(std::istream& input, FrontierSaveData& data, std::size_t count) {
    if (count > game::inventory::Inventory::slotCount) return false;
    data.inventory.clear();
    for (std::size_t i = 0; i < count; ++i) {
        std::size_t slotIndex = 0;
        int item = 0;
        unsigned countValue = 0;
        input >> slotIndex >> item >> countValue;
        if (!input || slotIndex >= game::inventory::Inventory::slotCount ||
            item < static_cast<int>(game::items::ItemId::None) ||
            item > static_cast<int>(game::items::ItemId::Leaves) || countValue > 65535u) return false;
        data.inventory.slot(slotIndex) = {
            static_cast<game::items::ItemId>(item), static_cast<std::uint16_t>(countValue)
        };
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
        } else if (token == "world_age") {
            input >> data.worldAgeSeconds;
        } else if (token == "player") {
            input >> data.playerPosition.x >> data.playerPosition.y >> data.playerPosition.z >> data.yaw >> data.pitch;
        } else if (token == "mining_mode") {
            int mode = 0;
            input >> mode;
            if (mode < static_cast<int>(game::mining::MiningMode::Block) ||
                mode > static_cast<int>(game::mining::MiningMode::Mixed)) return std::nullopt;
            data.miningMode = static_cast<game::mining::MiningMode>(mode);
        } else if (token == "inventory_selected") {
            std::size_t selected = 0;
            input >> selected;
            data.inventory.selectHotbar(selected);
        } else if (token == "inventory") {
            std::size_t count = 0;
            input >> count;
            if (!readInventory(input, data, count)) return std::nullopt;
        } else if (token == "edits") {
            std::size_t count = 0;
            input >> count;
            if (!readInlineEdits(input, data, count)) return std::nullopt;
        } else if (token == "region_store") {
            int enabled = 0;
            input >> enabled;
            if (enabled != 0) {
                auto regionEdits = worldstore::RegionFile::readAll(path.parent_path() / "regions");
                data.edits.insert(data.edits.end(), regionEdits.begin(), regionEdits.end());
            }
        } else if (token == "micro_region_store") {
            int enabled = 0;
            input >> enabled;
            if (enabled != 0) {
                data.microEdits = worldstore::MicroRegionFile::readAll(path.parent_path() / "micro-regions");
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

    if (!worldstore::RegionFile::writeAll(path.parent_path() / "regions", data.edits)) return false;
    if (!worldstore::MicroRegionFile::writeAll(path.parent_path() / "micro-regions", data.microEdits)) return false;

    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << signature << ' ' << currentSchemaVersion << '\n';
    output << "seed " << data.seed << '\n';
    output << std::setprecision(9) << "world_age " << data.worldAgeSeconds << '\n';
    output << std::setprecision(9) << "player "
           << data.playerPosition.x << ' ' << data.playerPosition.y << ' ' << data.playerPosition.z << ' '
           << data.yaw << ' ' << data.pitch << '\n';
    output << "mining_mode " << static_cast<int>(data.miningMode) << '\n';
    output << "inventory_selected " << data.inventory.selectedHotbar() << '\n';

    std::size_t usedSlots = 0;
    for (const auto& stack : data.inventory.slots()) if (!stack.empty()) ++usedSlots;
    output << "inventory " << usedSlots << '\n';
    for (std::size_t i = 0; i < data.inventory.slots().size(); ++i) {
        const auto& stack = data.inventory.slot(i);
        if (stack.empty()) continue;
        output << i << ' ' << static_cast<int>(stack.item) << ' ' << stack.count << '\n';
    }
    output << "region_store 1\n";
    output << "micro_region_store 1\n";
    output.flush();
    if (!output) return false;
    output.close();
    return replaceFile(temporary, path);
}

} // namespace rf::save
