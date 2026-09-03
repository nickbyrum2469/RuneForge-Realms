#pragma once

#ifdef _WIN32

#include "ui/native/NativeUiSurface.h"
#include "ui/theme/RuneForgePalette.h"

namespace rf::ui::theme {

inline void gem(native::NativeUiSurface& surface, float cx, float cy, float radius = 8.0f) {
    surface.diamond(cx, cy, radius + 3.0f, color(BronzeDeep), color(GoldDark), 1.4f);
    surface.diamond(cx, cy, radius, color(BlueGem), color(GoldBright), 1.6f);
    surface.diamond(cx - radius * 0.18f, cy - radius * 0.24f, radius * 0.36f,
                    color(BlueCore, 0.88f), color(BlueGlow, 0.88f), 0.8f);
}

inline void divider(native::NativeUiSurface& surface, float x0, float x1, float y) {
    surface.line(x0, y, x1, y, color(BronzeDeep), 4.0f);
    surface.line(x0, y - 0.5f, x1, y - 0.5f, color(Gold, 0.78f), 1.1f);
    gem(surface, (x0 + x1) * 0.5f, y, 5.0f);
}

inline void carvedPanel(native::NativeUiSurface& surface, native::UiRect rect, float radius = 8.0f) {
    surface.fill(rect, color(Panel, 0.995f), radius);
    surface.stroke(rect, color(BronzeDeep, 0.98f), 8.0f, radius);
    surface.stroke({rect.x + 7, rect.y + 7, rect.w - 14, rect.h - 14}, color(Bronze, 0.94f), 2.2f,
                   radius > 4.0f ? radius - 3.0f : 0.0f);
    surface.stroke({rect.x + 12, rect.y + 12, rect.w - 24, rect.h - 24}, color(Gold, 0.48f), 0.9f,
                   radius > 5.0f ? radius - 5.0f : 0.0f);
    gem(surface, rect.x + rect.w * 0.5f, rect.y, 6.0f);
    gem(surface, rect.x + rect.w * 0.5f, rect.y + rect.h, 6.0f);
}

inline void titlePlaque(native::NativeUiSurface& surface, native::UiRect rect,
                        const wchar_t* title, const wchar_t* subtitle = nullptr) {
    surface.fill(rect, color(PanelRaised, 0.995f), 5.0f);
    surface.stroke(rect, color(BronzeDeep), 6.0f, 5.0f);
    surface.stroke({rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 10}, color(Gold, 0.88f), 1.6f, 3.0f);
    gem(surface, rect.x + rect.w * 0.5f, rect.y, 7.0f);
    surface.text(title, {rect.x + 18, rect.y + 7, rect.w - 36, subtitle ? rect.h * 0.58f : rect.h - 14},
                 surface.titleFormat(), color(GoldBright));
    if (subtitle) {
        surface.text(subtitle, {rect.x + 18, rect.y + rect.h * 0.60f, rect.w - 36, rect.h * 0.26f},
                     surface.smallFormat(), color(BlueGlow));
    }
}

inline void menuButton(native::NativeUiSurface& surface, native::UiRect rect,
                       const wchar_t* label, bool danger = false) {
    surface.fill(rect, color(PanelRaised, 0.995f), 3.0f);
    surface.stroke(rect, color(BronzeDeep), 5.0f, 3.0f);
    surface.stroke({rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 10}, color(Gold, 0.74f), 1.2f, 2.0f);
    surface.fill({rect.x + 8, rect.y + 8, 52, rect.h - 16}, color(PanelInset), 2.0f);
    surface.stroke({rect.x + 8, rect.y + 8, 52, rect.h - 16}, color(Bronze, 0.82f), 1.1f, 2.0f);
    gem(surface, rect.x + 34, rect.y + rect.h * 0.5f, 5.5f);
    surface.text(label, {rect.x + 66, rect.y, rect.w - 76, rect.h}, surface.headingFormat(),
                 color(danger ? Danger : Ivory));
}

inline void inventorySlot(native::NativeUiSurface& surface, native::UiRect rect, bool selected = false) {
    surface.fill(rect, color(PanelInset, 0.995f), 3.0f);
    surface.stroke(rect, color(selected ? GoldBright : BronzeDeep), selected ? 3.0f : 2.4f, 3.0f);
    surface.stroke({rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8}, color(selected ? Gold : Bronze, 0.62f), 0.8f, 2.0f);
}

} // namespace rf::ui::theme

#endif
