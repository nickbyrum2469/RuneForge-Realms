#pragma once

#include "game/Math.h"
#include "game/inventory/Inventory.h"
#include "game/mining/MiningMode.h"
#include "world/WorldEdit.h"
#include "world/micro/MicroVoxelEdit.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace rf::save {

struct FrontierSaveData {
    std::uint32_t seed{1337};
    float worldAgeSeconds{};
    game::Vec3 playerPosition{0.5f, 10.0f, 0.5f};
    float yaw{};
    float pitch{-0.08f};
    game::mining::MiningMode miningMode{game::mining::MiningMode::Mixed};
    game::inventory::Inventory inventory{};
    std::vector<world::BlockEdit> edits;
    std::vector<world::micro::MicroVoxelEdit> microEdits;
};

[[nodiscard]] bool frontierSaveExists(const std::filesystem::path& path) noexcept;
[[nodiscard]] std::optional<FrontierSaveData> loadFrontierSave(const std::filesystem::path& path);
bool saveFrontierSave(const std::filesystem::path& path, const FrontierSaveData& data);

} // namespace rf::save
