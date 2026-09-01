#ifdef _WIN32

#include "ui/HubPainter.h"

#include <algorithm>
#include <array>
#include <string>

namespace rf::ui {
namespace {

D2D1_COLOR_F color(unsigned hex, float alpha = 1.0f) {
    return D2D1::ColorF(
        static_cast<float>((hex >> 16) & 0xff) / 255.0f,
        static_cast<float>((hex >> 8) & 0xff) / 255.0f,
        static_cast<float>(hex & 0xff) / 255.0f,
        alpha);
}

std::wstring wide(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

} // namespace

HubPainter::HubPainter(HWND hwnd) : hwnd_(hwnd) {}
HubPainter::~HubPainter() = default;

bool HubPainter::initialize() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 d2dFactory_.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(dwriteFactory_.ReleaseAndGetAddressOf())))) return false;
    createTextResources();
    createDeviceResources();
    return target_ != nullptr;
}

void HubPainter::createTextResources() {
    auto make = [this](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                       ComPtr<IDWriteTextFormat>& output) {
        dwriteFactory_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, size, L"en-us",
                                         output.ReleaseAndGetAddressOf());
        if (output) {
            output->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            output->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    };
    make(L"Bahnschrift", 60.0f, DWRITE_FONT_WEIGHT_BLACK, logoFormat_);
    make(L"Bahnschrift", 32.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, titleFormat_);
    make(L"Segoe UI", 19.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, headingFormat_);
    make(L"Segoe UI", 15.0f, DWRITE_FONT_WEIGHT_NORMAL, bodyFormat_);
    make(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, smallFormat_);
}

void HubPainter::createDeviceResources() {
    if (target_) return;
    RECT rect{};
    GetClientRect(hwnd_, &rect);
    pixelWidth_ = static_cast<unsigned>(std::max(1L, rect.right - rect.left));
    pixelHeight_ = static_cast<unsigned>(std::max(1L, rect.bottom - rect.top));
    d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(pixelWidth_, pixelHeight_)),
        target_.ReleaseAndGetAddressOf());
}

void HubPainter::discardDeviceResources() { target_.Reset(); }

void HubPainter::resize(unsigned width, unsigned height) {
    pixelWidth_ = std::max(1u, width);
    pixelHeight_ = std::max(1u, height);
    if (target_) target_->Resize(D2D1::SizeU(pixelWidth_, pixelHeight_));
}

float HubPainter::scaleX() const noexcept { return static_cast<float>(pixelWidth_) / HubLayout::width; }
float HubPainter::scaleY() const noexcept { return static_cast<float>(pixelHeight_) / HubLayout::height; }

void HubPainter::fillRect(const Rect& rect, D2D1_COLOR_F value, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(value, brush.ReleaseAndGetAddressOf());
    const auto d2dRect = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0.0f) {
        target_->FillRoundedRectangle(D2D1::RoundedRect(d2dRect, radius, radius), brush.Get());
    } else {
        target_->FillRectangle(d2dRect, brush.Get());
    }
}

void HubPainter::strokeRect(const Rect& rect, D2D1_COLOR_F value, float thickness, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(value, brush.ReleaseAndGetAddressOf());
    const auto d2dRect = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0.0f) {
        target_->DrawRoundedRectangle(D2D1::RoundedRect(d2dRect, radius, radius), brush.Get(), thickness);
    } else {
        target_->DrawRectangle(d2dRect, brush.Get(), thickness);
    }
}

void HubPainter::text(std::wstring_view value, const Rect& rect, IDWriteTextFormat* format,
                      D2D1_COLOR_F valueColor, DWRITE_TEXT_ALIGNMENT alignment) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(valueColor, brush.ReleaseAndGetAddressOf());
    format->SetTextAlignment(alignment);
    target_->DrawTextW(value.data(), static_cast<UINT32>(value.size()), format,
                       D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
}

void HubPainter::blockCluster(float x, float y, float width, float height,
                              D2D1_COLOR_F base, int seed) {
    constexpr float cell = 10.0f;
    for (int row = 0; row < static_cast<int>(height / cell); ++row) {
        for (int col = 0; col < static_cast<int>(width / cell); ++col) {
            const int variation = (seed * 29 + row * 17 + col * 11) % 9;
            if (row < 2 && variation < 2) continue;
            auto tint = base;
            const float delta = (variation - 4) * 0.015f;
            tint.r = std::clamp(tint.r + delta, 0.0f, 1.0f);
            tint.g = std::clamp(tint.g + delta, 0.0f, 1.0f);
            tint.b = std::clamp(tint.b + delta, 0.0f, 1.0f);
            fillRect({x + col * cell, y + row * cell, cell - 0.7f, cell - 0.7f}, tint);
        }
    }
}

void HubPainter::drawBackdrop() {
    fillRect({0, 0, 1600, 900}, color(0x07101d));
    fillRect({170, 0, 1430, 470}, color(0x18335c));
    fillRect({900, 0, 700, 470}, color(0x281b50, 0.72f));
    fillRect({170, 470, 1430, 430}, color(0x050a11, 0.92f));
}

void HubPainter::drawScenery() {
    blockCluster(560, 155, 540, 190, color(0x4d5654, 0.95f), 19);
    fillRect({635, 118, 60, 170}, color(0x6d6758));
    fillRect({810, 92, 74, 205}, color(0x756e5f));
    fillRect({980, 132, 52, 160}, color(0x655f58));
    blockCluster(205, 380, 1000, 150, color(0x355238, 0.95f), 7);
    fillRect({1190, 70, 230, 220}, color(0x244a79, 0.72f), 12);
    fillRect({1430, 95, 120, 200}, color(0x321757, 0.9f), 12);
    strokeRect({1450, 125, 80, 125}, color(0xd548ff), 7.0f, 34);
}

void HubPainter::drawLeftRail() {
    fillRect(layout_.leftRail, color(0x050b14, 0.96f));
    constexpr std::array labels{L"HOME", L"MODES", L"PARTY", L"LOCKER", L"SHOP", L"SETTINGS"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const bool active = model_.selectedNavIndex() == i;
        if (active) {
            fillRect(layout_.nav[i], color(0x172334), 8);
            strokeRect(layout_.nav[i], color(0xe0b65b), 1.5f, 8);
        }
        fillRect({layout_.nav[i].x + 14, layout_.nav[i].y + 22, 18, 18},
                 active ? color(0xf0bf54) : color(0x8994a1), 3);
        text(labels[i], {layout_.nav[i].x + 42, layout_.nav[i].y, 94, layout_.nav[i].h},
             headingFormat_.Get(), active ? color(0xf7d98e) : color(0xb4bdc8));
    }
}

void HubPainter::drawBrand() {
    text(L"RUNEFORGE", {470, 40, 630, 76}, logoFormat_.Get(), color(0xf2f4f7), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"R E A L M S", {610, 110, 350, 35}, headingFormat_.Get(), color(0xe1bb69), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({785, 142, 30, 30}, color(0x236fe1), 5);
}

void HubPainter::drawProfileStrip() {
    fillRect({1030, 18, 540, 72}, color(0x07101d, 0.88f), 8);
    fillRect({1048, 30, 48, 48}, color(0x276eb7), 8);
    text(L"RF", {1048, 30, 48, 48}, headingFormat_.Get(), color(0xffffff), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"Forgekeeper", {1110, 28, 145, 26}, headingFormat_.Get(), color(0xf4f5f7));
    text(L"Online", {1110, 54, 90, 18}, smallFormat_.Get(), color(0x78dd69));
    fillRect({1280, 30, 120, 42}, color(0x151d29), 5);
    text(L"12,450", {1280, 30, 120, 42}, bodyFormat_.Get(), color(0xf1d181), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({1410, 30, 120, 42}, color(0x151d29), 5);
    text(L"2,180", {1410, 30, 120, 42}, bodyFormat_.Get(), color(0xc991ff), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawFeatured() {
    fillRect(layout_.featured, color(0x070b10, 0.76f), 8);
    strokeRect(layout_.featured, color(0xb68c42), 1.4f, 8);
    const auto& mode = model_.selectedMode();
    text(L"FEATURED REALM", {220, 260, 260, 25}, smallFormat_.Get(), color(0x4ca8ff));
    text(wide(mode.title), {220, 290, 440, 52}, titleFormat_.Get(), color(0xf6f6f6));
    text(wide(mode.subtitle), {220, 342, 400, 28}, bodyFormat_.Get(), color(0xc5cbd3));
    text(wide(mode.description), {220, 380, 430, 88}, bodyFormat_.Get(), color(0xe0e3e7));
    fillRect({690, 260, 520, 320}, color(0x101923, 0.54f), 8);
    blockCluster(725, 410, 440, 145, color(0x405641), static_cast<int>(model_.selectedModeIndex()) + 9);
    blockCluster(820, 330, 245, 130, color(0x646052), static_cast<int>(model_.selectedModeIndex()) + 23);
    fillRect(layout_.playButton, color(0xf0b742), 8);
    strokeRect(layout_.playButton, color(0xffe49a), 2.0f, 8);
    text(L"PLAY", layout_.playButton, titleFormat_.Get(), color(0x2b1a06), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawPartyPanel() {
    fillRect(layout_.partyPanel, color(0x070d17, 0.93f), 8);
    strokeRect(layout_.partyPanel, color(0x475566), 1, 8);
    text(L"PARTY & FRIENDS", {1275, 258, 210, 28}, smallFormat_.Get(), color(0xc1c8d1));
    text(L"2 / 4", {1490, 258, 55, 28}, smallFormat_.Get(), color(0xffffff), DWRITE_TEXT_ALIGNMENT_CENTER);
    for (int i = 0; i < 4; ++i) {
        const Rect avatar{1278.0f + i * 62.0f, 302, 48, 48};
        fillRect(avatar, i < 2 ? color(i == 0 ? 0x347dc2 : 0x2799ad) : color(0x111925), 7);
        text(i < 2 ? L"RF" : L"+", avatar, headingFormat_.Get(), color(0xffffff), DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    constexpr std::array names{L"SkyBuilder", L"BrickMaster", L"PixelKnight", L"NeonNinja"};
    for (int i = 0; i < 4; ++i) {
        fillRect({1275, 395.0f + i * 43.0f, 282, 36}, color(0x0e1621), 4);
        text(names[i], {1290, 395.0f + i * 43.0f, 180, 36}, smallFormat_.Get(), color(0xe4e7eb));
        fillRect({1535, 409.0f + i * 43.0f, 8, 8}, color(0x70dd63), 4);
    }
}

void HubPainter::drawModeCards() {
    text(L"EXPLORE REALMS", {195, 612, 210, 28}, smallFormat_.Get(), color(0xc6ccd4));
    constexpr std::array<unsigned, 6> palette{0x28627f, 0x27745f, 0x56366f, 0x66543b, 0x765e35, 0x332c68};
    const auto& modes = model_.modes();
    for (std::size_t i = 0; i < layout_.cards.size(); ++i) {
        const auto& rect = layout_.cards[i];
        const bool active = i == model_.selectedModeIndex();
        fillRect(rect, color(0x0a1119), 7);
        strokeRect(rect, active ? color(0xe1ae4d) : color(0x4b5662), active ? 2.0f : 1.0f, 7);
        fillRect({rect.x + 2, rect.y + 2, rect.w - 4, 118}, color(palette[i]), 6);
        blockCluster(rect.x + 10, rect.y + 68, rect.w - 20, 50, color(palette[i] + 0x101010), static_cast<int>(i) * 13 + 3);
        text(wide(modes[i].title), {rect.x + 10, rect.y + 124, rect.w - 20, 30}, headingFormat_.Get(), color(0xf0f1f3));
        text(wide(modes[i].subtitle), {rect.x + 10, rect.y + 154, rect.w - 20, 22}, smallFormat_.Get(), color(0xaeb8c4));
        text(wide(modes[i].tag), {rect.x + 10, rect.y + 178, rect.w - 20, 20}, smallFormat_.Get(), color(0x78bfff));
    }
}

void HubPainter::drawNewsBar() {
    fillRect(layout_.newsBar, color(0x0b1219), 4);
    text(L"NEWS & UPDATES", {205, 868, 140, 26}, smallFormat_.Get(), color(0xb9c1cb));
    text(L"NATIVE FOUNDATION", {365, 868, 150, 26}, smallFormat_.Get(), color(0x55a9ff));
    text(L"RuneForge Realms 0.1.0 - native shell, release channel and automatic update bootstrapper online.",
         {525, 868, 930, 26}, smallFormat_.Get(), color(0xaab3bf));
    text(L"v0.1.0", {1480, 868, 85, 26}, smallFormat_.Get(), color(0x818b96), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::draw() {
    createDeviceResources();
    if (!target_) return;
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scaleX(), scaleY()));
    target_->Clear(color(0x060b12));
    drawBackdrop();
    drawScenery();
    drawLeftRail();
    drawBrand();
    drawProfileStrip();
    drawFeatured();
    drawPartyPanel();
    drawModeCards();
    drawNewsBar();
    const HRESULT result = target_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) discardDeviceResources();
}

HubAction HubPainter::click(float pixelX, float pixelY) {
    const float x = pixelX / scaleX();
    const float y = pixelY / scaleY();
    if (layout_.playButton.contains(x, y)) return HubAction::Play;
    for (std::size_t i = 0; i < layout_.nav.size(); ++i) {
        if (!layout_.nav[i].contains(x, y)) continue;
        model_.selectNav(i);
        InvalidateRect(hwnd_, nullptr, FALSE);
        constexpr std::array actions{HubAction::None, HubAction::OpenModes, HubAction::OpenParty,
                                     HubAction::OpenLocker, HubAction::OpenShop, HubAction::OpenSettings};
        return actions[i];
    }
    for (std::size_t i = 0; i < layout_.cards.size(); ++i) {
        if (!layout_.cards[i].contains(x, y)) continue;
        model_.selectMode(i);
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    }
    return HubAction::None;
}

} // namespace rf::ui

#endif
