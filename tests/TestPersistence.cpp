#include "TestSuites.h"

#include "save/FrontierSave.h"
#include "save/world/MicroRegionFile.h"
#include "save/world/RegionFile.h"
#include "world/micro/MicroVoxelEdit.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::size_t fileCount(const std::filesystem::path& directory, std::string_view extension) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return 0;
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == extension) ++count;
    }
    return count;
}

} // namespace

void runPersistenceTests() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "runeforge-0.4.0-persistence-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    using rf::save::worldstore::RegionFile;
    assert((RegionFile::regionForBlock(0, 0) == rf::save::worldstore::RegionCoord{0, 0}));
    assert((RegionFile::regionForBlock(600, 0) == rf::save::worldstore::RegionCoord{1, 0}));
    assert((RegionFile::regionForBlock(-1, 0) == rf::save::worldstore::RegionCoord{-1, 0}));

    const fs::path currentDir = root / "current";
    const fs::path savePath = currentDir / "world.rfsv";
    rf::save::FrontierSaveData data;
    data.seed = 424242u;
    data.worldAgeSeconds = 137.5f;
    data.playerPosition = {12.5f, 7.0f, -4.5f};
    data.yaw = 0.75f;
    data.pitch = -0.2f;
    data.miningMode = rf::game::mining::MiningMode::Micro;
    assert(data.inventory.add(rf::game::items::ItemId::StoneBlock, 23) == 0);
    data.inventory.selectHotbar(0);
    data.edits = {
        {{1, 5, 1}, rf::world::BlockId::Air},
        {{600, 6, 1}, rf::world::BlockId::Stone},
        {{-1, 7, 1}, rf::world::BlockId::Wood},
    };

    rf::world::micro::MicroVoxelState chipped;
    const auto removed = chipped.clearSphere({7, 7, 7}, 1);
    assert(removed > 0);
    data.microEdits.push_back(rf::world::micro::makeEdit({1, 5, 2}, rf::world::BlockId::Stone, chipped));

    assert(rf::save::saveFrontierSave(savePath, data));
    assert(fileCount(currentDir / "regions", ".rfr") == 3);
    assert(fileCount(currentDir / "micro-regions", ".rfm") == 1);
    const auto loaded = rf::save::loadFrontierSave(savePath);
    assert(loaded);
    assert(loaded->seed == data.seed);
    assert(loaded->worldAgeSeconds == data.worldAgeSeconds);
    assert(loaded->miningMode == rf::game::mining::MiningMode::Micro);
    assert(loaded->inventory.slot(0).item == rf::game::items::ItemId::StoneBlock);
    assert(loaded->inventory.slot(0).count == 23);
    assert(loaded->edits.size() == data.edits.size());
    assert(loaded->microEdits.size() == 1);
    assert(loaded->microEdits.front().occupancyWords == data.microEdits.front().occupancyWords);

    // A full save removes obsolete region and micro-region files rather than resurrecting stale edits.
    data.edits.resize(1);
    data.microEdits.clear();
    assert(rf::save::saveFrontierSave(savePath, data));
    assert(fileCount(currentDir / "regions", ".rfr") == 1);
    assert(fileCount(currentDir / "micro-regions", ".rfm") == 0);
    const auto trimmed = rf::save::loadFrontierSave(savePath);
    assert(trimmed && trimmed->edits.size() == 1 && trimmed->microEdits.empty());

    // Existing 0.3.x schema-1 saves remain loadable, then migrate forward to schema 3.
    const fs::path legacyDir = root / "legacy";
    fs::create_directories(legacyDir);
    const fs::path legacyPath = legacyDir / "world.rfsv";
    {
        std::ofstream legacy(legacyPath, std::ios::trunc);
        legacy << "RUNEFORGE_FRONTIER_SAVE 1\n";
        legacy << "seed 99\n";
        legacy << "player 0.5 8 0.5 0.25 -0.1\n";
        legacy << "edits 1\n";
        legacy << "-1 4 2 " << static_cast<int>(rf::world::BlockId::Stone) << "\n";
    }

    const auto oldSave = rf::save::loadFrontierSave(legacyPath);
    assert(oldSave && oldSave->seed == 99u && oldSave->edits.size() == 1);
    assert(oldSave->miningMode == rf::game::mining::MiningMode::Mixed);
    assert(rf::save::saveFrontierSave(legacyPath, *oldSave));
    assert(fileCount(legacyDir / "regions", ".rfr") == 1);

    std::ifstream migrated(legacyPath);
    std::string firstLine;
    std::getline(migrated, firstLine);
    assert(firstLine == "RUNEFORGE_FRONTIER_SAVE 3");

    fs::remove_all(root, ec);
}
