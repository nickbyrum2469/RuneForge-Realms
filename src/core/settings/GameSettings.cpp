#include "core/settings/GameSettings.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace rf::core::settings {

void GameSettings::sanitize() noexcept {
    mouseSensitivity = std::clamp(mouseSensitivity, 0.25f, 2.50f);
    fovDegrees = std::clamp(fovDegrees, 65.0f, 110.0f);
    foliageQuality = std::clamp(foliageQuality, 0, 2);
}

GameSettings loadGameSettings(const std::filesystem::path& path) {
    GameSettings settings;
    std::ifstream input(path);
    std::string key;
    while (input >> key) {
        if (key == "mouse_sensitivity") input >> settings.mouseSensitivity;
        else if (key == "fov_degrees") input >> settings.fovDegrees;
        else if (key == "foliage_quality") input >> settings.foliageQuality;
        else {
            std::string ignored;
            std::getline(input, ignored);
        }
    }
    settings.sanitize();
    return settings;
}

bool saveGameSettings(const std::filesystem::path& path, const GameSettings& source) {
    GameSettings settings = source;
    settings.sanitize();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) return false;
        output << "mouse_sensitivity " << settings.mouseSensitivity << '\n';
        output << "fov_degrees " << settings.fovDegrees << '\n';
        output << "foliage_quality " << settings.foliageQuality << '\n';
        if (!output) return false;
    }
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

} // namespace rf::core::settings
