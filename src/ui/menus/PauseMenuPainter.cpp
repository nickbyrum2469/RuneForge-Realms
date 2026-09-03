#ifdef _WIN32

#include "ui/menus/PauseMenuPainter.h"

#include "ui/theme/RuneForgeChrome.h"
#include "ui/theme/RuneForgePalette.h"

namespace rf::ui::menus {

void PauseMenuPainter::draw() {
    using namespace rf::ui::theme;
    if (!surface_.begin()) return;

    // The modal owns a separate DWM surface, so paint an intentional dark game-menu backdrop rather
    // than the flat blue developer screen. Layered silhouettes echo the supplied sunset-menu framing.
    surface_.fill({0, 0, 1600, 900}, color(Void));
    surface_.fill({0, 0, 1600, 235}, color(0x10141b, 0.94f));
    surface_.fill({0, 650, 1600, 250}, color(0x05070a, 0.98f));
    for (int i = 0; i < 8; ++i) {
        const float x = static_cast<float>(i) * 220.0f - 70.0f;
        const float h = 90.0f + static_cast<float>((i * 37) % 90);
        surface_.fill({x, 650.0f - h, 165.0f, h}, color(0x0b1016, 0.72f));
    }

    const native::UiRect frame{495, 82, 610, 690};
    carvedPanel(surface_, frame, 7.0f);
    titlePlaque(surface_, {530, 112, 540, 138}, L"RUNEFORGE", L"REALMS  /  FRONTIER PAUSED");

    divider(surface_, 555, 1045, 282);
    menuButton(surface_, resume_, L"RESUME");
    menuButton(surface_, settings_, L"SETTINGS");
    menuButton(surface_, mainMenu_, L"SAVE & RETURN TO REALMS");
    menuButton(surface_, quit_, L"SAVE & QUIT", true);

    divider(surface_, 555, 1045, 672);
    surface_.text(L"ESC  RETURN TO FRONTIER", {585, 697, 430, 26}, surface_.smallFormat(), color(Muted));
    surface_.text(L"RUNE FORGED. WORLD REMEMBERED.", {585, 724, 430, 22}, surface_.smallFormat(), color(Gold, 0.72f));

    surface_.end();
}

PauseAction PauseMenuPainter::hitTest(int pixelX, int pixelY) const noexcept {
    const auto [x, y] = surface_.logicalPoint(pixelX, pixelY);
    if (resume_.contains(x,y)) return PauseAction::Resume;
    if (settings_.contains(x,y)) return PauseAction::Settings;
    if (mainMenu_.contains(x,y)) return PauseAction::ReturnToMain;
    if (quit_.contains(x,y)) return PauseAction::Quit;
    return PauseAction::None;
}

} // namespace rf::ui::menus

#endif
