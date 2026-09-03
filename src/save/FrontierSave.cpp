#include "save/FrontierSave.h"

#include "save/TransactionalFileReplace.h"
#include "save/world/MicroRegionFile.h"
#include "save/world/RegionFile.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <string>

namespace rf::save {
namespace {
constexpr const char* signature = "RUNEFORGE_FRONTIER_SAVE";
constexpr int currentSchemaVersion = 3;

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

bool validMiningDamage(const game::mining::MiningDamageState& state) noexcept {
    return std::isfinite(state.progress) && state.progress > 0.0f && state.progress < 1.0f;
}

bool readMiningDamage(std::istream& input, FrontierSaveData& data, std::size_t count) {
    if (count > 1000000u) return false;
    data.miningDamage.clear();
    data.miningDamage.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        game::mining::MiningDamageState state;
        input >> state.position.x >> state.position.y >> state.position.z >> state.progress;
        if (!input || !validMiningDamage(state)) return false;
        data.miningDamage.push_back(state);
    }
    return true;
}

bool readMicroHarvest(std::istream& input, FrontierSaveData& data, std::size_t count) {
    if (count > FrontierSaveData::microHarvestSlots) return false;
    data.microHarvestCells.fill(0);
    for (std::size_t i = 0; i < count; ++i) {
        int block = 0;
        std::uint32_t cells = 0;
        input >> block >> cells;
        if (!input || block <= static_cast<int>(world::BlockId::Air) ||
            block > static_cast<int>(world::BlockId::Leaves) || cells >= world::micro::cellCount) return false;
        data.microHarvestCells[static_cast<std::size_t>(block)] = cells;
    }
    return true;
}

bool validDrop(const game::drops::WorldDrop& drop) noexcept {
    return drop.item > game::items::ItemId::None && drop.item <= game::items::ItemId::Leaves &&
           drop.count > 0 &&
           std::isfinite(drop.position.x) && std::isfinite(drop.position.y) && std::isfinite(drop.position.z) &&
           std::isfinite(drop.velocity.x) && std::isfinite(drop.velocity.y) && std::isfinite(drop.velocity.z) &&
           std::isfinite(drop.age) && drop.age >= 0.0f;
}

bool readDrops(std::istream& input, FrontierSaveData& data, std::size_t count) {
    if (count > 100000u) return false;
    data.drops.clear();
    data.drops.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        game::drops::WorldDrop drop;
        int item = 0;
        unsigned countValue = 0;
        input >> drop.id >> item >> countValue
              >> drop.position.x >> drop.position.y >> drop.position.z
              >> drop.velocity.x >> drop.velocity.y >> drop.velocity.z >> drop.age;
        if (!input || item <= static_cast<int>(game::items::ItemId::None) ||
            item > static_cast<int>(game::items::ItemId::Leaves) || countValue == 0 || countValue > 65535u) return false;
        drop.item = static_cast<game::items::ItemId>(item);
        drop.count = static_cast<std::uint16_t>(countValue);
        if (!validDrop(drop)) return false;
        data.drops.push_back(drop);
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
        } else if (token == "mining_damage") {
            std::size_t count = 0;
            input >> count;
            if (!readMiningDamage(input, data, count)) return std::nullopt;
        } else if (token == "micro_harvest") {
            std::size_t count = 0;
            input >> count;
            if (!readMicroHarvest(input, data, count)) return std::nullopt;
        } else if (token == "drops") {
            std::size_t count = 0;
            input >> count;
            if (!readDrops(input, data, count)) return std::nullopt;
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
            if (enabled != 0) data.microEdits = worldstore::MicroRegionFile::readAll(path.parent_path() / "micro-regions");
        }
        if (!input) return std::nullopt;
    }

    if (!std::isfinite(data.worldAgeSeconds) || data.worldAgeSeconds < 0.0f ||
        !std::isfinite(data.playerPosition.x) || !std::isfinite(data.playerPosition.y) ||
        !std::isfinite(data.playerPosition.z) || !std::isfinite(data.yaw) || !std::isfinite(data.pitch)) {
        return std::nullopt;
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

    std::size_t damageCount = 0;
    for (const auto& state : data.miningDamage) if (validMiningDamage(state)) ++damageCount;
    output << "mining_damage " << damageCount << '\n';
    for (const auto& state : data.miningDamage) {
        if (!validMiningDamage(state)) continue;
        output << state.position.x << ' ' << state.position.y << ' ' << state.position.z << ' '
               << std::setprecision(9) << state.progress << '\n';
    }

    std::size_t harvestTypes = 0;
    for (std::size_t block = 1; block < data.microHarvestCells.size(); ++block) {
        if ((data.microHarvestCells[block] % world::micro::cellCount) != 0) ++harvestTypes;
    }
    output << "micro_harvest " << harvestTypes << '\n';
    for (std::size_t block = 1; block < data.microHarvestCells.size(); ++block) {
        const std::uint32_t cells = data.microHarvestCells[block] % world::micro::cellCount;
        if (cells == 0) continue;
        output << block << ' ' << cells << '\n';
    }

    std::size_t dropCount = 0;
    for (const auto& drop : data.drops) if (validDrop(drop)) ++dropCount;
    output << "drops " << dropCount << '\n';
    for (const auto& drop : data.drops) {
        if (!validDrop(drop)) continue;
        output << drop.id << ' ' << static_cast<int>(drop.item) << ' ' << drop.count << ' '
               << std::setprecision(9)
               << drop.position.x << ' ' << drop.position.y << ' ' << drop.position.z << ' '
               << drop.velocity.x << ' ' << drop.velocity.y << ' ' << drop.velocity.z << ' '
               << drop.age << '\n';
    }

    output << "region_store 1\n";
    output << "micro_region_store 1\n";
    output.flush();
    if (!output) return false;
    output.close();
    return detail::replaceFileRollbackSafe(temporary, path);
}

} // namespace rf::save
