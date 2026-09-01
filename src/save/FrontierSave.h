#pragma once

#include "game/Math.h"
#include "world/WorldEdit.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace rf::save {

struct FrontierSaveData {
    std::uint32_t seed{1337};
    game::Vec3 playerPosition{0.5f, 10.0f, 0.5f};
    float yaw{};
    float pitch{-0.08f};
    std::vector<world::BlockEdit> edits;
};

[[nodiscard]] bool frontierSaveExists(const std::filesystem::path& path) noexcept;
[[nodiscard]] std::optional<FrontierSaveData> loadFrontierSave(const std::filesystem::path& path);
bool saveFrontierSave(const std::filesystem::path& path, const FrontierSaveData& data);

} // namespace rf::save
