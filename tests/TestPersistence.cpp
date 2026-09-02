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
    using rf::save::worldstore::MicroRegionFile;
    assert((RegionFile::regionForBlock(0, 0) == rf::save::worldstore::RegionCoord{0, 0}));
    assert((RegionFile::regionForBlock(600, 0) == rf::save::worldstore::RegionCoord{1, 0}));
    assert((RegionFile::regionForBlock(-1, 0) == rf::save::worldstore::RegionCoord{-1, 0}));

    // Region stores are rewritten frequently as player edits evolve. Replacing an already-existing
    // file must commit the new payload without leaving a stale rollback file. On Windows this drives
    // the rename-over-existing fallback path that previously deleted the live region first.
    const fs::path replaceRegionDir = root / "replace-regions";
    std::vector<rf::world::BlockEdit> replaceEdits{{{4, 8, 4}, rf::world::BlockId::Stone}};
    assert(RegionFile::writeAll(replaceRegionDir, replaceEdits));
    replaceEdits.front().block = rf::world::BlockId::Wood;
    assert(RegionFile::writeAll(replaceRegionDir, replaceEdits));
    const auto replacedEdits = RegionFile::readAll(replaceRegionDir);
    assert(replacedEdits.size() == 1 && replacedEdits.front().block == rf::world::BlockId::Wood);
    const auto regionPath = RegionFile::pathFor(replaceRegionDir, RegionFile::regionForBlock(4, 4));
    assert(!fs::exists(regionPath.string() + ".bak"));

    const fs::path replaceMicroDir = root / "replace-micro-regions";
    rf::world::micro::MicroVoxelState firstMicroState;
    assert(firstMicroState.clearSphere({7, 7, 7}, 1) > 0);
    std::vector<rf::world::micro::MicroVoxelEdit> replaceMicroEdits{
        rf::world::micro::makeEdit({4, 8, 4}, rf::world::BlockId::Stone, firstMicroState)
    };
    assert(MicroRegionFile::writeAll(replaceMicroDir, replaceMicroEdits));
    rf::world::micro::MicroVoxelState secondMicroState;
    assert(secondMicroState.clearSphere({4, 4, 4}, 2) > 0);
    replaceMicroEdits.front() = rf::world::micro::makeEdit({4, 8, 4}, rf::world::BlockId::Stone, secondMicroState);
    assert(MicroRegionFile::writeAll(replaceMicroDir, replaceMicroEdits));
    const auto replacedMicroEdits = MicroRegionFile::readAll(replaceMicroDir);
    assert(replacedMicroEdits.size() == 1);
    assert(replacedMicroEdits.front().occupancyWords == replaceMicroEdits.front().occupancyWords);
    const auto microPath = MicroRegionFile::pathFor(replaceMicroDir, RegionFile::regionForBlock(4, 4));
    assert(!fs::exists(microPath.string() + ".bak"));

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
    data.miningDamage = {
        {{4, 9, 4}, 0.42f},
        {{5, 9, 4}, 0.78f},
    };
    data.microHarvestCells[static_cast<std::size_t>(rf::world::BlockId::Stone)] = 377;
    data.microHarvestCells[static_cast<std::size_t>(rf::world::BlockId::Dirt)] = 41;
    data.drops.push_back({19, rf::game::items::ItemId::OakLog, 2,
                          {3.5f, 8.2f, -1.5f}, {0.3f, 1.1f, -0.2f}, 3.0f});
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
    assert(loaded->miningDamage.size() == 2);
    assert(loaded->miningDamage[0].position == data.miningDamage[0].position);
    assert(loaded->miningDamage[0].progress == data.miningDamage[0].progress);
    assert(loaded->microHarvestCells[static_cast<std::size_t>(rf::world::BlockId::Stone)] == 377);
    assert(loaded->microHarvestCells[static_cast<std::size_t>(rf::world::BlockId::Dirt)] == 41);
    assert(loaded->drops.size() == 1);
    assert(loaded->drops.front().id == 19);
    assert(loaded->drops.front().item == rf::game::items::ItemId::OakLog);
    assert(loaded->drops.front().count == 2);
    assert(loaded->edits.size() == data.edits.size());
    assert(loaded->microEdits.size() == 1);
    assert(loaded->microEdits.front().occupancyWords == data.microEdits.front().occupancyWords);

    // Serializer record counts must describe the lines actually emitted, even if a caller hands
    // save code invalid/transient state. Invalid records are omitted rather than corrupting parsing.
    data.miningDamage.push_back({{6, 9, 4}, 0.0f});
    data.drops.push_back({20, rf::game::items::ItemId::None, 0, {}, {}, 0.0f});
    assert(rf::save::saveFrontierSave(savePath, data));
    const auto sanitized = rf::save::loadFrontierSave(savePath);
    assert(sanitized && sanitized->miningDamage.size() == 2 && sanitized->drops.size() == 1);

    // A full save removes obsolete region and micro-region files rather than resurrecting stale edits.
    data.edits.resize(1);
    data.microEdits.clear();
    data.miningDamage.clear();
    data.microHarvestCells.fill(0);
    data.drops.clear();
    assert(rf::save::saveFrontierSave(savePath, data));
    assert(fileCount(currentDir / "regions", ".rfr") == 1);
    assert(fileCount(currentDir / "micro-regions", ".rfm") == 0);
    const auto trimmed = rf::save::loadFrontierSave(savePath);
    assert(trimmed && trimmed->edits.size() == 1 && trimmed->microEdits.empty());
    assert(trimmed->miningDamage.empty() && trimmed->drops.empty());

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
    assert(oldSave->miningDamage.empty() && oldSave->drops.empty());
    assert(rf::save::saveFrontierSave(legacyPath, *oldSave));
    assert(fileCount(legacyDir / "regions", ".rfr") == 1);

    std::ifstream migrated(legacyPath);
    std::string firstLine;
    std::getline(migrated, firstLine);
    assert(firstLine == "RUNEFORGE_FRONTIER_SAVE 3");

    fs::remove_all(root, ec);
}
