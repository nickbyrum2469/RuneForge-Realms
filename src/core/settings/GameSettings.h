#pragma once

#include <filesystem>

namespace rf::core::settings {

struct GameSettings {
    float mouseSensitivity{1.0f};
    float fovDegrees{78.0f};
    int foliageQuality{2}; // 0 low, 1 medium, 2 high

    void sanitize() noexcept;
};

[[nodiscard]] GameSettings loadGameSettings(const std::filesystem::path& path);
[[nodiscard]] bool saveGameSettings(const std::filesystem::path& path, const GameSettings& settings);

} // namespace rf::core::settings
