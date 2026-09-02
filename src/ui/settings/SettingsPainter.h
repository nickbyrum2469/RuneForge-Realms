#pragma once

#ifdef _WIN32

#include "core/settings/GameSettings.h"
#include "ui/native/NativeUiSurface.h"

#include <windows.h>

namespace rf::ui::settings {

enum class SettingsAction {
    None,
    SensitivityDown,
    SensitivityUp,
    FovDown,
    FovUp,
    FoliageCycle,
    Back,
};

class SettingsPainter {
public:
    explicit SettingsPainter(HWND hwnd) : surface_(hwnd) {}
    bool initialize() { return surface_.initialize(); }
    void resize(unsigned width, unsigned height) { surface_.resize(width, height); }
    void setSettings(const core::settings::GameSettings& settings) noexcept { settings_ = settings; }
    void draw();
    [[nodiscard]] SettingsAction hitTest(int pixelX, int pixelY) const noexcept;

private:
    static constexpr native::UiRect sensitivityMinus_{870, 294, 54, 54};
    static constexpr native::UiRect sensitivityPlus_{1050, 294, 54, 54};
    static constexpr native::UiRect fovMinus_{870, 386, 54, 54};
    static constexpr native::UiRect fovPlus_{1050, 386, 54, 54};
    static constexpr native::UiRect foliage_{870, 478, 234, 54};
    static constexpr native::UiRect back_{650, 626, 300, 62};

    native::NativeUiSurface surface_;
    core::settings::GameSettings settings_{};
};

} // namespace rf::ui::settings

#endif
