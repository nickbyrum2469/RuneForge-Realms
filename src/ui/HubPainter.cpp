#ifdef _WIN32

#include "ui/HubPainter.h"

#include "core/Version.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace rf::ui {
namespace {

D2D1_COLOR_F rgb(unsigned hex, float alpha = 1.0f) {
    return D2D1::ColorF(
        static_cast<float>((hex >> 16) & 0xff) / 255.0f,
        static_cast<float>((hex >> 8) & 0xff) / 255.0f,
        static_cast<float>(hex & 0xff) / 255.0f,
        alpha);
}

std::wstring widen(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

} // namespace

HubPainter::HubPainter(HWND hwnd) : hwnd_(hwnd) {}
HubPainter::~HubPainter() = default;

bool HubPainter::initialize() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())))) return false;
    createTextResources();
    createDeviceResources();
    return target_ != nullptr;
}

void HubPainter::createTextResources() {
    auto make = [&](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight, ComPtr<IDWriteTextFormat>& out) {
        dwriteFactory_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", out.GetAddressOf());
        if (out) {
            out->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            out->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    };
    make(L"Bahnschrift", 64.0f, DWRITE_FONT_WEIGHT_BLACK, logoFormat_);
    make(L"Bahnschrift", 33.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, titleFormat_);
    make(L"Segoe UI", 20.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, headingFormat_);
    make(L"Segoe UI", 15.0f, DWRITE_FONT_WEIGHT_NORMAL, bodyFormat_);
    make(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, smallFormat_);
}

void HubPainter::createDeviceResources() {
    if (target_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    pixelWidth_ = std::max(1L, rc.right - rc.left);
    pixelHeight_ = std::max(1L, rc.bottom - rc.top);
    const auto size = D2D1::SizeU(pixelWidth_, pixelHeight_);
    d2dFactory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                        D2D1::HwndRenderTargetProperties(hwnd_, size), target_.GetAddressOf());
}

void HubPainter::discardDeviceResources() { target_.Reset(); }

void HubPainter::resize(unsigned width, unsigned height) {
    pixelWidth_ = std::max(1u, width);
    pixelHeight_ = std::max(1u, height);
    if (target_) target_->Resize(D2D1::SizeU(pixelWidth_, pixelHeight_));
}

float HubPainter::scaleX() const noexcept { return static_cast<float>(pixelWidth_) / HubLayout::width; }
float HubPainter::scaleY() const noexcept { return static_cast<float>(pixelHeight_) / HubLayout::height; }

void HubPainter::fillRect(const Rect& r, D2D1_COLOR_F color, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.GetAddressOf());
    if (radius > 0) {
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), radius, radius), brush.Get());
    } else {
        target_->FillRectangle(D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), brush.Get());
    }
}

void HubPainter::strokeRect(const Rect& r, D2D1_COLOR_F color, float thickness, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.GetAddressOf());
    if (radius > 0) {
        target_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), radius, radius), brush.Get(), thickness);
    } else {
        target_->DrawRectangle(D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), brush.Get(), thickness);
    }
}

void HubPainter::text(std::wstring_view value, const Rect& r, IDWriteTextFormat* format, D2D1_COLOR_F color,
                      DWRITE_TEXT_ALIGNMENT alignment) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.GetAddressOf());
    format->SetTextAlignment(alignment);
    target_->DrawTextW(value.data(), static_cast<UINT32>(value.size()), format,
                       D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), brush.Get(),
                       D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void HubPainter::drawBackdrop() {
    std::array<D2D1_GRADIENT_STOP, 4> stops{{
        {0.0f, rgb(0x0b1426)}, {0.38f, rgb(0x132d54)}, {0.72f, rgb(0x261947)}, {1.0f, rgb(0x070b13)}};
    ComPtr<ID2D1GradientStopCollection> collection;
    target_->CreateGradientStopCollection(stops.data(), static_cast<UINT32>(stops.size()), collection.GetAddressOf());
    ComPtr<ID2D1LinearGradientBrush> brush;
    target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(170, 40), D2D1::Point2F(1580, 830)),
                                       collection.Get(), brush.GetAddressOf());
    target_->FillRectangle(D2D1::RectF(0, 0, HubLayout::width, HubLayout::height), brush.Get());

    fillRect({170, 0, 1430, 900}, rgb(0x07101c, 0.20f));
}

void HubPainter::blockCluster(float x, float y, float w, float h, D2D1_COLOR_F base, int seed) {
    const float cell = 8.0f;
    for (int row = 0; row < static_cast<int>(h / cell); ++row) {
        for (int col = 0; col < static_cast<int>(w / cell); ++col) {
            const int v = (seed * 31 + row * 17 + col * 13) % 11;
            if (v < 2 && row < 2) continue;
            auto c = base;
            const float shift = (static_cast<float>(v) - 5.0f) * 0.014f;
            c.r = std::clamp(c.r + shift, 0.0f, 1.0f);
            c.g = std::clamp(c.g + shift, 0.0f, 1.0f);
            c.b = std::clamp(c.b + shift, 0.0f, 1.0f);
            fillRect({x + col * cell, y + row * cell, cell - 0.6f, cell - 0.6f}, c);
        }
    }
}

void HubPainter::drawScenery() {
    fillRect({205, 92, 260, 72}, rgb(0x19233d, 0.82f), 8);
    blockCluster(220, 112, 220, 80, rgb(0x42516e, 0.8f), 4);
    fillRect({305, 67, 42, 60}, rgb(0x5a4661, 0.9f));
    fillRect({355, 82, 30, 45}, rgb(0x70504e, 0.9f));

    for (int i = 0; i < 8; ++i) {
        const float x = 1010.0f + i * 55.0f;
        const float peak = 105.0f + (i % 3) * 42.0f;
        ComPtr<ID2D1PathGeometry> geo;
        d2dFactory_->CreatePathGeometry(geo.GetAddressOf());
        ComPtr<ID2D1GeometrySink> sink;
        geo->Open(sink.GetAddressOf());
        sink->BeginFigure(D2D1::Point2F(x, 240), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(x + 38, peak));
        sink->AddLine(D2D1::Point2F(x + 76, 240));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();
        ComPtr<ID2D1SolidColorBrush> brush;
        target_->CreateSolidColorBrush(rgb(0x244a79, 0.72f), brush.GetAddressOf());
        target_->FillGeometry(geo.Get(), brush.Get());
    }

    fillRect({1430, 116, 115, 165}, rgb(0x21143d, 0.74f), 10);
    strokeRect({1453, 145, 70, 105}, rgb(0xc43cff, 0.9f), 8.0f, 34);
    fillRect({1466, 165, 44, 70}, rgb(0x5121a5, 0.35f), 20);

    blockCluster(585, 165, 435, 150, rgb(0x425061, 0.72f), 12);
    fillRect({640, 125, 54, 130}, rgb(0x505b70, 0.88f));
    fillRect({820, 112, 68, 155}, rgb(0x546079, 0.9f));
    fillRect({948, 143, 46, 118}, rgb(0x4c5870, 0.88f));
    fillRect({170, 480, 1430, 420}, rgb(0x050a11, 0.54f));
}

void HubPainter::drawLeftRail() {
    fillRect(layout_.leftRail, rgb(0x050b14, 0.91f));
    strokeRect({169, 0, 1, 900}, rgb(0x324b65, 0.75f));
    constexpr std::array labels{L"HOME", L"MODES", L"PARTY", L"LOCKER", L"SHOP", L"SETTINGS"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const bool active = model_.selectedNavIndex() == i;
        if (active) {
            fillRect(layout_.nav[i], rgb(0x182334, 0.96f), 8);
            strokeRect(layout_.nav[i], rgb(0xe3b354, 0.95f), 1.5f, 8);
        }
        text(labels[i], {layout_.nav[i].x + 42, layout_.nav[i].y, 92, layout_.nav[i].h}, headingFormat_.Get(),
             active ? rgb(0xf6d686) : rgb(0xb5bbc4));
        const float cx = layout_.nav[i].x + 24;
        const float cy = layout_.nav[i].y + 22;
        fillRect({cx - 8, cy, 16, 16}, active ? rgb(0xf2c25b) : rgb(0x8a929d), 2);
    }
    fillRect({30, 786, 108, 70}, rgb(0x0c1420, 0.92f), 7);
    strokeRect({30, 786, 108, 70}, rgb(0x495666, 0.7f), 1, 7);
    text(L"NEWS\nCOMMUNITY", {38, 794, 92, 52}, smallFormat_.Get(), rgb(0x9fa8b4), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawBrand() {
    text(L"RUNEFORGE", {485, 45, 610, 78}, logoFormat_.Get(), rgb(0xf2f4f7), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"R E A L M S", {610, 112, 360, 42}, headingFormat_.Get(), rgb(0xd5b66d), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({785, 145, 30, 30}, rgb(0x1a66d9), 4);
    strokeRect({780, 140, 40, 40}, rgb(0x77b9ff), 2, 5);
}

void HubPainter::drawProfileStrip() {
    fillRect({1040, 18, 530, 72}, rgb(0x07101d, 0.72f), 8);
    fillRect({1058, 30, 48, 48}, rgb(0x2268b9), 8);
    text(L"RF", {1058, 30, 48, 48}, headingFormat_.Get(), rgb(0xf8fbff), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"Forgekeeper", {1120, 28, 132, 26}, headingFormat_.Get(), rgb(0xf1f3f6));
    text(L"● Online", {1120, 52, 120, 20}, smallFormat_.Get(), rgb(0x7fe46d));
    fillRect({1280, 29, 120, 43}, rgb(0x151c29, 0.94f), 5);
    text(L"◉  12,450", {1290, 29, 100, 43}, bodyFormat_.Get(), rgb(0xf1d386), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({1410, 29, 120, 43}, rgb(0x151c29, 0.94f), 5);
    text(L"◆  2,180", {1420, 29, 100, 43}, bodyFormat_.Get(), rgb(0xc98cff), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawFeatured() {
    fillRect(layout_.featured, rgb(0x070b10, 0.70f), 8);
    strokeRect(layout_.featured, rgb(0xb6893b, 0.78f), 1.4f, 8);
    const auto& mode = model_.selectedMode();
    text(L"FEATURED REALM", {220, 260, 300, 28}, smallFormat_.Get(), rgb(0x45a5ff));
    text(widen(mode.title), {220, 292, 500, 52}, titleFormat_.Get(), rgb(0xf5f5f5));
    text(widen(mode.subtitle), {220, 342, 380, 30}, bodyFormat_.Get(), rgb(0xc9ced5));
    text(widen(mode.description), {220, 380, 440, 92}, bodyFormat_.Get(), rgb(0xe0e3e7));
    fillRect({220, 470, 88, 28}, rgb(0x1a3c58, 0.9f), 4);
    text(widen(mode.tag), {220, 470, 88, 28}, smallFormat_.Get(), rgb(0x8fcaff), DWRITE_TEXT_ALIGNMENT_CENTER);

    fillRect({685, 260, 525, 320}, rgb(0x101b29, 0.46f), 7);
    blockCluster(730, 405, 420, 145, rgb(0x314438, 0.90f), static_cast<int>(model_.selectedModeIndex()) + 7);
    blockCluster(825, 315, 235, 145, rgb(0x5c5a51, 0.92f), static_cast<int>(model_.selectedModeIndex()) + 19);
    fillRect({920, 300, 44, 165}, rgb(0x6d6658, 0.95f));
    fillRect({785, 350, 35, 115}, rgb(0x5a5c58, 0.94f));
    fillRect({1040, 365, 34, 100}, rgb(0x625c54, 0.95f));
    for (int i = 0; i < 6; ++i) fillRect({800.0f + i * 58.0f, 455, 10, 10}, rgb(0xf5a83d, 0.9f), 2);

    fillRect(layout_.playButton, rgb(0xf0b742), 8);
    strokeRect(layout_.playButton, rgb(0xffe7a0), 2.0f, 8);
    text(L"▶   PLAY", layout_.playButton, titleFormat_.Get(), rgb(0x2b1a06), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({458, 510, 62, 70}, rgb(0x151b22, 0.92f), 7);
    text(L"≡", {458, 510, 62, 70}, titleFormat_.Get(), rgb(0xd7dbe0), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawPartyPanel() {
    fillRect(layout_.partyPanel, rgb(0x070d17, 0.88f), 8);
    strokeRect(layout_.partyPanel, rgb(0x475566, 0.7f), 1, 8);
    text(L"PARTY & FRIENDS", {1275, 258, 180, 28}, smallFormat_.Get(), rgb(0xbec6d1));
    text(L"2 / 4", {1470, 258, 70, 28}, smallFormat_.Get(), rgb(0xf4f5f7), DWRITE_TEXT_ALIGNMENT_CENTER);
    for (int i = 0; i < 4; ++i) {
        Rect avatar{1278.0f + i * 62.0f, 302, 48, 48};
        fillRect(avatar, i < 2 ? rgb(i == 0 ? 0x3b83c9 : 0x2b9fb1) : rgb(0x111925), 7);
        strokeRect(avatar, rgb(0x3d4d60), 1, 7);
        text(i < 2 ? L"RF" : L"+", avatar, headingFormat_.Get(), i < 2 ? rgb(0xffffff) : rgb(0x8f9baa), DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    text(L"FRIENDS ONLINE — 4", {1275, 368, 220, 24}, smallFormat_.Get(), rgb(0x9ea8b4));
    constexpr std::array names{L"SkyBuilder", L"BrickMaster", L"PixelKnight", L"NeonNinja"};
    constexpr std::array places{L"In Lobby", L"Frontier Realms", L"Echo Depths", L"Forgekeeper"};
    for (int i = 0; i < 4; ++i) {
        fillRect({1275, 400.0f + i * 44.0f, 282, 38}, rgb(0x0e1621, 0.82f), 4);
        fillRect({1284, 407.0f + i * 44.0f, 25, 25}, rgb(0x245f91 + i * 0x070707), 4);
        text(names[i], {1318, 401.0f + i * 44.0f, 140, 18}, smallFormat_.Get(), rgb(0xe3e6eb));
        text(places[i], {1318, 419.0f + i * 44.0f, 185, 16}, smallFormat_.Get(), rgb(0x8bcf6c));
        fillRect({1538, 414.0f + i * 44.0f, 8, 8}, rgb(0x70dd63), 4);
    }
    fillRect({1275, 571, 282, 28}, rgb(0x151d27), 4);
    text(L"FIND FRIENDS", {1275, 571, 282, 28}, smallFormat_.Get(), rgb(0xc6ccd4), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawModeCards() {
    text(L"EXPLORE REALMS", {195, 612, 220, 28}, smallFormat_.Get(), rgb(0xc6ccd4));
    const auto& modes = model_.modes();
    for (std::size_t i = 0; i < layout_.cards.size(); ++i) {
        const auto& r = layout_.cards[i];
        const bool active = i == model_.selectedModeIndex();
        fillRect(r, rgb(0x0a1119, 0.94f), 7);
        strokeRect(r, active ? rgb(0xe0ad4d) : rgb(0x4b5662), active ? 2.0f : 1.0f, 7);
        const unsigned palettes[] = {0x265978, 0x2a765e, 0x59386f, 0x5c4c33, 0x6d5633, 0x352c68};
        fillRect({r.x + 2, r.y + 2, r.w - 4, 118}, rgb(palettes[i], 0.98f), 6);
        blockCluster(r.x + 12, r.y + 62, r.w - 24, 56, rgb(palettes[i] + 0x151515, 0.9f), static_cast<int>(i) * 11 + 3);
        text(widen(modes[i].title), {r.x + 10, r.y + 124, r.w - 20, 30}, headingFormat_.Get(), rgb(0xf0f1f3));
        text(widen(modes[i].subtitle), {r.x + 10, r.y + 154, r.w - 20, 22}, smallFormat_.Get(), rgb(0xaeb8c4));
        fillRect({r.x + r.w - 82, r.y + 177, 70, 20}, rgb(0x152638), 3);
        text(widen(modes[i].tag), {r.x + r.w - 82, r.y + 177, 70, 20}, smallFormat_.Get(), rgb(0x80bfff), DWRITE_TEXT_ALIGNMENT_CENTER);
    }
}

void HubPainter::drawNewsBar() {
    fillRect(layout_.newsBar, rgb(0x0b1219, 0.95f), 4);
    text(L"NEWS & UPDATES", {205, 868, 140, 26}, smallFormat_.Get(), rgb(0xb9c1cb));
    text(L"NATIVE FOUNDATION", {365, 868, 145, 26}, smallFormat_.Get(), rgb(0x55a9ff));
    text(L"RuneForge Realms 0.1.0 — native C++ shell, release channel and automatic update bootstrapper online.",
         {520, 868, 950, 26}, smallFormat_.Get(), rgb(0xaab3bf));
    text(L"v0.1.0", {1485, 868, 80, 26}, smallFormat_.Get(), rgb(0x7f8995), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::draw() {
    createDeviceResources();
    if (!target_) return;

    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scaleX(), scaleY()));
    target_->Clear(rgb(0x060b12));
    drawBackdrop();
    drawScenery();
    drawLeftRail();
    drawBrand();
    drawProfileStrip();
    drawFeatured();
    drawPartyPanel();
    drawModeCards();
    drawNewsBar();

    const auto hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) discardDeviceResources();
}

HubAction HubPainter::click(float pixelX, float pixelY) {
    const float x = pixelX / scaleX();
    const float y = pixelY / scaleY();
    if (layout_.playButton.contains(x, y)) return HubAction::Play;

    for (std::size_t i = 0; i < layout_.nav.size(); ++i) {
        if (!layout_.nav[i].contains(x, y)) continue;
        model_.selectNav(i);
        InvalidateRect(hwnd_, nullptr, FALSE);
        switch (i) {
            case 1: return HubAction::OpenModes;
            case 2: return HubAction::OpenParty;
            case 3: return HubAction::OpenLocker;
            case 4: return HubAction::OpenShop;
            case 5: return HubAction::OpenSettings;
            default: return HubAction::None;
        }
    }
    for (std::size_t i = 0; i < layout_.cards.size(); ++i) {
        if (layout_.cards[i].contains(x, y)) {
            model_.selectMode(i);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return HubAction::None;
        }
    }
    return HubAction::None;
}

} // namespace rf::ui

#endif
