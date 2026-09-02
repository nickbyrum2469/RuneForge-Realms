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
    static constexpr native::UiRect resume_{600, 304, 400, 68};
    static constexpr native::UiRect settings_{600, 388, 400, 68};
    static constexpr native::UiRect mainMenu_{600, 472, 400, 68};
    static constexpr native::UiRect quit_{600, 556, 400, 68};

    native::NativeUiSurface surface_;
};

} // namespace rf::ui::menus

#endif
