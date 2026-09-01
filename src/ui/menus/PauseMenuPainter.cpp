#ifdef _WIN32

#include "ui/menus/PauseMenuPainter.h"

#include "ui/theme/RuneForgePalette.h"

namespace rf::ui::menus {
namespace {
using rf::ui::theme::color;

void button(native::NativeUiSurface& surface, native::UiRect rect, const wchar_t* label) {
    surface.fill(rect, color(theme::PanelRaised, 0.98f), 8);
    surface.stroke(rect, color(theme::BronzeDark, 0.96f), 5.0f, 8);
    surface.stroke({rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 10}, color(theme::Gold, 0.72f), 1.4f, 6);
    surface.text(label, rect, surface.headingFormat(), color(theme::Ivory));
}
}

void PauseMenuPainter::draw() {
    if (!surface_.begin()) return;
    surface_.fill({0,0,1600,900}, color(0x04070d));
    surface_.fill({0,0,1600,900}, color(0x142238, 0.32f));
    surface_.fill({480,105,640,635}, color(theme::Panel, 0.985f), 18);
    surface_.stroke({480,105,640,635}, color(theme::BronzeDark, 0.98f), 8.0f, 18);
    surface_.stroke({491,116,618,613}, color(theme::Gold, 0.76f), 1.7f, 13);
    surface_.fill({565,145,470,82}, color(0x101927, 0.98f), 12);
    surface_.stroke({565,145,470,82}, color(theme::Gold, 0.85f), 2.0f, 12);
    surface_.text(L"RUNEFO RGE REALMS", {565,146,470,44}, surface_.titleFormat(), color(theme::Ivory));
    surface_.text(L"FRONTIER REALMS  /  PAUSED", {565,190,470,28}, surface_.smallFormat(), color(theme::BlueGlow));

    button(surface_, resume_, L"RESUME");
    button(surface_, settings_, L"SETTINGS");
    button(surface_, mainMenu_, L"SAVE & RETURN TO MAIN MENU");
    button(surface_, quit_, L"SAVE & QUIT");
    surface_.text(L"ESC  RESUME", {600,660,400,30}, surface_.smallFormat(), color(theme::Muted));
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
