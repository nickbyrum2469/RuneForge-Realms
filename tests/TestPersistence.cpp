#include "TestSuites.h"

#include "save/FrontierSave.h"
#include "save/world/RegionFile.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::size_t regionFileCount(const std::filesystem::path& directory) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return 0;
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == ".rfr") ++count;
    }
    return count;
}

} // namespace

void runPersistenceTests() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "runeforge-0.3.2-persistence-test";
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
    data.playerPosition = {12.5f, 7.0f, -4.5f};
    data.yaw = 0.75f;
    data.pitch = -0.2f;
    data.edits = {
        {{1, 5, 1}, rf::world::BlockId::Air},
        {{600, 6, 1}, rf::world::BlockId::Stone},
        {{-1, 7, 1}, rf::world::BlockId::Wood},
    };

    assert(rf::save::saveFrontierSave(savePath, data));
    assert(regionFileCount(currentDir / "regions") == 3);
    const auto loaded = rf::save::loadFrontierSave(savePath);
    assert(loaded);
    assert(loaded->seed == data.seed);
    assert(loaded->edits.size() == data.edits.size());

    // A full save must remove obsolete region files rather than resurrecting stale edits later.
    data.edits.resize(1);
    assert(rf::save::saveFrontierSave(savePath, data));
    assert(regionFileCount(currentDir / "regions") == 1);
    const auto trimmed = rf::save::loadFrontierSave(savePath);
    assert(trimmed && trimmed->edits.size() == 1);

    // Existing 0.3.0/0.3.1 schema-1 saves remain loadable, then migrate to region storage.
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
    assert(rf::save::saveFrontierSave(legacyPath, *oldSave));
    assert(regionFileCount(legacyDir / "regions") == 1);

    std::ifstream migrated(legacyPath);
    std::string firstLine;
    std::getline(migrated, firstLine);
    assert(firstLine == "RUNEFORGE_FRONTIER_SAVE 2");

    fs::remove_all(root, ec);
}
