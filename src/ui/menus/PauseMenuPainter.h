#pragma once

#ifdef _WIN32

#include "ui/native/NativeUiSurface.h"

#include <windows.h>

namespace rf::ui::menus {

enum class PauseAction { None, Resume, Settings, ReturnToMain, Quit };

class PauseMenuPainter {
public:
    explicit PauseMenuPainter(HWND hwnd) : surface_(hwnd) {}
    bool initialize() { return surface_.initialize(); }
    void resize(unsigned width, unsigned height) { surface_.resize(width, height); }
    void draw();
    [[nodiscard]] PauseAction hitTest(int pixelX, int pixelY) const noexcept;

private:
    static constexpr native::UiRect resume_{570, 330, 460, 66};
    static constexpr native::UiRect settings_{570, 410, 460, 66};
    static constexpr native::UiRect mainMenu_{570, 490, 460, 66};
    static constexpr native::UiRect quit_{570, 570, 460, 66};

    native::NativeUiSurface surface_;
};

} // namespace rf::ui::menus

#endif
