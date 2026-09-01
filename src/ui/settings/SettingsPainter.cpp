#ifdef _WIN32

#include "ui/settings/SettingsPainter.h"

#include "ui/theme/RuneForgePalette.h"

#include <iomanip>
#include <sstream>

namespace rf::ui::settings {
namespace {
using rf::ui::theme::color;

void control(native::NativeUiSurface& surface, native::UiRect rect, const wchar_t* label) {
    surface.fill(rect, color(theme::PanelRaised, 0.98f), 7);
    surface.stroke(rect, color(theme::Bronze, 0.68f), 1.5f, 7);
    surface.text(label, rect, surface.headingFormat(), color(theme::Ivory));
}

std::wstring sensitivityText(float value) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << L"x";
    return stream.str();
}

std::wstring fovText(float value) {
    return std::to_wstring(static_cast<int>(value + 0.5f)) + L" deg";
}

const wchar_t* foliageText(int quality) {
    return quality <= 0 ? L"LOW" : (quality == 1 ? L"MEDIUM" : L"HIGH");
}
}

void SettingsPainter::draw() {
    if (!surface_.begin()) return;
    surface_.fill({0,0,1600,900}, color(theme::Void));
    surface_.fill({0,0,1600,900}, color(0x13253c, 0.28f));
    surface_.fill({390,92,820,650}, color(theme::Panel, 0.99f), 18);
    surface_.stroke({390,92,820,650}, color(theme::BronzeDark, 0.98f), 8.0f, 18);
    surface_.stroke({401,103,798,628}, color(theme::Gold, 0.74f), 1.6f, 13);

    surface_.text(L"SETTINGS", {520,130,560,60}, surface_.titleFormat(), color(theme::Ivory));
    surface_.text(L"These controls are live and persist between sessions.", {520,190,560,28},
                  surface_.bodyFormat(), color(theme::Muted));

    surface_.text(L"MOUSE SENSITIVITY", {500,292,330,56}, surface_.headingFormat(), color(theme::Ivory), DWRITE_TEXT_ALIGNMENT_LEADING);
    control(surface_, sensitivityMinus_, L"-");
    surface_.text(sensitivityText(settings_.mouseSensitivity), {930,294,114,54}, surface_.headingFormat(), color(theme::BlueGlow));
    control(surface_, sensitivityPlus_, L"+");

    surface_.text(L"FIELD OF VIEW", {500,384,330,56}, surface_.headingFormat(), color(theme::Ivory), DWRITE_TEXT_ALIGNMENT_LEADING);
    control(surface_, fovMinus_, L"-");
    surface_.text(fovText(settings_.fovDegrees), {930,386,114,54}, surface_.headingFormat(), color(theme::BlueGlow));
    control(surface_, fovPlus_, L"+");

    surface_.text(L"FOLIAGE QUALITY", {500,476,330,56}, surface_.headingFormat(), color(theme::Ivory), DWRITE_TEXT_ALIGNMENT_LEADING);
    control(surface_, foliage_, foliageText(settings_.foliageQuality));
    surface_.text(L"More display, audio, controls and accessibility settings will use this same persistent system.",
                  {500,550,600,32}, surface_.smallFormat(), color(theme::Muted));

    control(surface_, back_, L"BACK");
    surface_.end();
}

SettingsAction SettingsPainter::hitTest(int pixelX, int pixelY) const noexcept {
    const auto [x, y] = surface_.logicalPoint(pixelX, pixelY);
    if (sensitivityMinus_.contains(x,y)) return SettingsAction::SensitivityDown;
    if (sensitivityPlus_.contains(x,y)) return SettingsAction::SensitivityUp;
    if (fovMinus_.contains(x,y)) return SettingsAction::FovDown;
    if (fovPlus_.contains(x,y)) return SettingsAction::FovUp;
    if (foliage_.contains(x,y)) return SettingsAction::FoliageCycle;
    if (back_.contains(x,y)) return SettingsAction::Back;
    return SettingsAction::None;
}

} // namespace rf::ui::settings

#endif
