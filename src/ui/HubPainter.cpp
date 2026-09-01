#ifdef _WIN32

#include "ui/HubPainter.h"

#include <algorithm>
#include <array>
#include <string>

namespace rf::ui {
namespace {

D2D1_COLOR_F color(unsigned hex, float alpha = 1.0f) {
    return D2D1::ColorF(static_cast<float>((hex >> 16) & 0xff) / 255.0f,
                        static_cast<float>((hex >> 8) & 0xff) / 255.0f,
                        static_cast<float>(hex & 0xff) / 255.0f, alpha);
}

std::wstring wide(std::string_view value) { return std::wstring(value.begin(), value.end()); }

std::wstring buildVersion() {
    std::wstring value;
    for (const char* p = RF_VERSION_STRING; *p; ++p) value.push_back(static_cast<wchar_t>(*p));
    return value;
}

} // namespace

HubPainter::HubPainter(HWND hwnd) : hwnd_(hwnd) {}
HubPainter::~HubPainter() = default;

bool HubPainter::initialize() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.ReleaseAndGetAddressOf()))) return false;
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
                                         DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", output.ReleaseAndGetAddressOf());
        if (output) {
            output->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            output->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    };
    make(L"Bahnschrift", 58.0f, DWRITE_FONT_WEIGHT_BLACK, logoFormat_);
    make(L"Bahnschrift", 31.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, titleFormat_);
    make(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, headingFormat_);
    make(L"Segoe UI", 14.0f, DWRITE_FONT_WEIGHT_NORMAL, bodyFormat_);
    make(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, smallFormat_);
    make(L"Segoe UI", 10.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, tinyFormat_);
}

void HubPainter::createDeviceResources() {
    if (target_) return;
    RECT rect{};
    GetClientRect(hwnd_, &rect);
    pixelWidth_ = static_cast<unsigned>(std::max(1L, rect.right - rect.left));
    pixelHeight_ = static_cast<unsigned>(std::max(1L, rect.bottom - rect.top));
    d2dFactory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(pixelWidth_, pixelHeight_)), target_.ReleaseAndGetAddressOf());
}

void HubPainter::discardDeviceResources() { target_.Reset(); }
void HubPainter::resize(unsigned width, unsigned height) {
    pixelWidth_ = std::max(1u, width); pixelHeight_ = std::max(1u, height);
    if (target_) target_->Resize(D2D1::SizeU(pixelWidth_, pixelHeight_));
}
float HubPainter::scaleX() const noexcept { return static_cast<float>(pixelWidth_) / HubLayout::width; }
float HubPainter::scaleY() const noexcept { return static_cast<float>(pixelHeight_) / HubLayout::height; }

void HubPainter::fillRect(const Rect& rect, D2D1_COLOR_F value, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(value, brush.ReleaseAndGetAddressOf());
    const auto d2d = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0) target_->FillRoundedRectangle(D2D1::RoundedRect(d2d, radius, radius), brush.Get());
    else target_->FillRectangle(d2d, brush.Get());
}

void HubPainter::strokeRect(const Rect& rect, D2D1_COLOR_F value, float thickness, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(value, brush.ReleaseAndGetAddressOf());
    const auto d2d = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0) target_->DrawRoundedRectangle(D2D1::RoundedRect(d2d, radius, radius), brush.Get(), thickness);
    else target_->DrawRectangle(d2d, brush.Get(), thickness);
}

void HubPainter::fillGradient(const Rect& rect, D2D1_COLOR_F top, D2D1_COLOR_F bottom) {
    const D2D1_GRADIENT_STOP stops[2]{{0.0f, top}, {1.0f, bottom}};
    ComPtr<ID2D1GradientStopCollection> collection;
    target_->CreateGradientStopCollection(stops, 2, collection.ReleaseAndGetAddressOf());
    ComPtr<ID2D1LinearGradientBrush> brush;
    target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
        D2D1::Point2F(rect.x, rect.y), D2D1::Point2F(rect.x, rect.y + rect.h)), collection.Get(), brush.ReleaseAndGetAddressOf());
    target_->FillRectangle(D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
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
                              D2D1_COLOR_F base, int seed, float cell) {
    for (int row = 0; row < static_cast<int>(height / cell); ++row) {
        for (int col = 0; col < static_cast<int>(width / cell); ++col) {
            const int variation = (seed * 29 + row * 17 + col * 11) % 11;
            if (variation == 0 && row < 3) continue;
            auto tint = base;
            const float delta = (variation - 5) * 0.012f;
            tint.r = std::clamp(tint.r + delta, 0.0f, 1.0f);
            tint.g = std::clamp(tint.g + delta, 0.0f, 1.0f);
            tint.b = std::clamp(tint.b + delta, 0.0f, 1.0f);
            fillRect({x + col * cell, y + row * cell, cell - 0.6f, cell - 0.6f}, tint);
        }
    }
}

void HubPainter::drawCastle(float x, float y, float scale, D2D1_COLOR_F stone) {
    fillRect({x, y + 55 * scale, 220 * scale, 95 * scale}, stone, 3);
    fillRect({x + 18 * scale, y + 20 * scale, 46 * scale, 130 * scale}, stone, 3);
    fillRect({x + 152 * scale, y + 10 * scale, 50 * scale, 140 * scale}, stone, 3);
    fillRect({x + 82 * scale, y + 38 * scale, 70 * scale, 112 * scale}, stone, 3);
    const auto light = color(0xffbf5c, 0.88f);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 6; ++col) {
            if ((row + col) % 2 == 0) fillRect({x + (20 + col * 30) * scale, y + (72 + row * 23) * scale,
                                                8 * scale, 11 * scale}, light, 2);
        }
    }
}

void HubPainter::drawBackdrop() {
    fillGradient({0, 0, 1600, 900}, color(0x0b1731), color(0x563b4b));
    fillGradient({170, 350, 1430, 550}, color(0x1b2940, 0.20f), color(0x040912, 0.98f));
}

void HubPainter::drawScenery() {
    // Clouds and atmospheric bands.
    for (int i = 0; i < 7; ++i) {
        fillRect({230.0f + i * 190.0f, 72.0f + (i % 3) * 34.0f, 120.0f, 22.0f}, color(0xd7cfcc, 0.12f), 11);
    }

    // Floating citadel left.
    blockCluster(205, 82, 310, 88, color(0x25304a, 0.92f), 14, 8);
    drawCastle(260, 22, 0.72f, color(0x595160, 0.92f));
    blockCluster(275, 168, 170, 62, color(0x27352e, 0.90f), 29, 7);
    fillRect({350, 215, 18, 95}, color(0x5ea5ff, 0.20f), 9);

    // Stepped snowy mountains and a distant blue glacier.
    for (int i = 0; i < 8; ++i) {
        const float w = 420.0f - i * 42.0f;
        fillRect({1020.0f + i * 22.0f, 250.0f - i * 24.0f, w, 32.0f},
                 i > 4 ? color(0xbdd8e9, 0.34f) : color(0x355879, 0.38f), 3);
    }

    // Valley city and river beneath the UI glass.
    blockCluster(430, 335, 770, 225, color(0x293a30, 0.76f), 41, 8);
    drawCastle(760, 300, 0.92f, color(0x514f49, 0.90f));
    fillRect({960, 430, 155, 230}, color(0x163c61, 0.38f), 70);
    fillRect({990, 430, 65, 230}, color(0x5da0d0, 0.12f), 32);

    // Neon portal hints at later Realmweave without pretending it is a playable mode.
    fillRect({1440, 105, 105, 170}, color(0x271447, 0.78f), 14);
    strokeRect({1458, 128, 70, 120}, color(0xd64bff, 0.92f), 7.0f, 30);
    strokeRect({1466, 138, 54, 101}, color(0x5f8dff, 0.45f), 3.0f, 25);
}

void HubPainter::drawLeftRail() {
    fillGradient(layout_.leftRail, color(0x050b15, 0.99f), color(0x07101a, 0.99f));
    constexpr std::array labels{L"HOME", L"WORLDS", L"SETTINGS", L"QUIT"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const bool active = model_.selectedNavIndex() == i;
        if (active) {
            fillRect(layout_.nav[i], color(0x18283c, 0.96f), 8);
            strokeRect(layout_.nav[i], color(0xe8bb56), 1.6f, 8);
            fillRect({layout_.nav[i].x, layout_.nav[i].y + 10, 3, layout_.nav[i].h - 20}, color(0xffd46d), 2);
        }
        fillRect({layout_.nav[i].x + 15, layout_.nav[i].y + 23, 18, 18}, active ? color(0xf6c75d) : color(0x7f8b9a), 4);
        text(labels[i], {layout_.nav[i].x + 43, layout_.nav[i].y, 92, layout_.nav[i].h}, headingFormat_.Get(),
             active ? color(0xffe4a1) : color(0xc1c8d2));
    }
    fillRect({30, 790, 110, 76}, color(0x0b1420, 0.94f), 8);
    text(L"NATIVE\nVULKAN", {30, 798, 110, 55}, tinyFormat_.Get(), color(0x7099c7), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawBrand() {
    text(L"RUNEFORGE", {500, 28, 620, 78}, logoFormat_.Get(), color(0xf3f4f6), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"R  E  A  L  M  S", {615, 98, 390, 34}, headingFormat_.Get(), color(0xf0c465), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({788, 131, 28, 28}, color(0x2a80e8), 6);
    strokeRect({782, 125, 40, 40}, color(0x8fc8ff, 0.45f), 1.5f, 8);
}

void HubPainter::drawProfileStrip() {
    fillRect({1025, 18, 545, 74}, color(0x07101c, 0.90f), 9);
    strokeRect({1025, 18, 545, 74}, color(0x36475c, 0.75f), 1, 9);
    fillRect({1043, 31, 48, 48}, color(0x287cc9), 9);
    text(L"RF", {1043, 31, 48, 48}, headingFormat_.Get(), color(0xffffff), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"Forgekeeper", {1106, 28, 150, 27}, headingFormat_.Get(), color(0xf4f5f7));
    text(L"Local Realm", {1106, 55, 120, 18}, smallFormat_.Get(), color(0x69dd7a));
    fillRect({1290, 31, 125, 42}, color(0x111b29), 6);
    text(L"FRONTIER", {1290, 31, 125, 42}, smallFormat_.Get(), color(0xf0c96c), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({1425, 31, 125, 42}, color(0x111b29), 6);
    text(L"BUILD " + buildVersion(), {1425, 31, 125, 42}, tinyFormat_.Get(), color(0x8dbbff), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawFeatured() {
    fillRect(layout_.featured, color(0x050a11, 0.82f), 10);
    strokeRect(layout_.featured, color(0xc59a45, 0.92f), 1.6f, 10);
    text(L"FLAGSHIP SURVIVAL REALM", {220, 252, 300, 24}, smallFormat_.Get(), color(0x51a7ff));
    text(L"Frontier Realms", {220, 285, 475, 52}, titleFormat_.Get(), color(0xf7f7f5));
    text(L"Persistent procedural survival", {220, 338, 420, 27}, bodyFormat_.Get(), color(0xd7c58f));
    text(L"Wake in a living voxel frontier. Explore, gather, carve and build your way from one block at a time toward structures, settlements and eventually the land itself.",
         {220, 378, 455, 92}, bodyFormat_.Get(), color(0xe2e6eb));

    fillRect({700, 260, 382, 305}, color(0x0b1722, 0.52f), 9);
    blockCluster(715, 395, 350, 160, color(0x36523a, 0.88f), 71, 7);
    drawCastle(790, 330, 0.78f, color(0x615d50, 0.90f));
    fillRect({725, 278, 105, 24}, color(0x11283a, 0.92f), 5);
    text(L"SURVIVAL", {725, 278, 105, 24}, tinyFormat_.Get(), color(0x6ac9ff), DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({840, 278, 105, 24}, color(0x25341f, 0.92f), 5);
    text(L"PERSISTENT", {840, 278, 105, 24}, tinyFormat_.Get(), color(0x8ee47e), DWRITE_TEXT_ALIGNMENT_CENTER);

    const bool canContinue = model_.hasSave();
    fillRect(layout_.continueButton, canContinue ? color(0x152c42) : color(0x111821), 8);
    strokeRect(layout_.continueButton, canContinue ? color(0x5b9bd3) : color(0x3a4652), 1.4f, 8);
    text(canContinue ? L"CONTINUE" : L"NO SAVE YET", layout_.continueButton, headingFormat_.Get(),
         canContinue ? color(0xd8efff) : color(0x6f7b88), DWRITE_TEXT_ALIGNMENT_CENTER);

    fillGradient(layout_.newGameButton, color(0xffcb58), color(0xe5a42f));
    strokeRect(layout_.newGameButton, color(0xffeda9), 2.0f, 8);
    text(L"NEW WORLD", layout_.newGameButton, headingFormat_.Get(), color(0x271a07), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawStatusPanel() {
    fillRect(layout_.statusPanel, color(0x07101b, 0.91f), 10);
    strokeRect(layout_.statusPanel, color(0x44576d, 0.86f), 1, 10);
    text(L"FRONTIER STATUS", {1160, 252, 210, 27}, smallFormat_.Get(), color(0xcbd3dd));
    text(model_.hasSave() ? L"WORLD READY" : L"NEW FRONTIER", {1375, 252, 165, 27}, smallFormat_.Get(),
         model_.hasSave() ? color(0x74df70) : color(0xf1c46b), DWRITE_TEXT_ALIGNMENT_CENTER);

    constexpr std::array labels{L"WORLD TYPE", L"RENDERER", L"SAVE SYSTEM", L"CURRENT MILESTONE"};
    constexpr std::array values{L"Procedural Survival", L"Vulkan 1.3", L"Local Persistent", L"Playable Foundation"};
    for (int i = 0; i < 4; ++i) {
        const float y = 303.0f + i * 58.0f;
        fillRect({1160, y, 390, 48}, color(0x0d1825, 0.88f), 6);
        text(labels[i], {1175, y, 155, 48}, tinyFormat_.Get(), color(0x70859b));
        text(values[i], {1320, y, 215, 48}, smallFormat_.Get(), color(0xe8edf3), DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    text(L"0.3: walk, jump, collide, break, place, save and return.", {1160, 545, 390, 38}, smallFormat_.Get(), color(0xe5b957));
}

void HubPainter::drawFeatureCards() {
    text(L"THE FRONTIER GROWS WITH YOU", {195, 612, 320, 28}, smallFormat_.Get(), color(0xd0d6dd));
    constexpr std::array titles{L"WILDLANDS", L"DEEP CAVERNS", L"ANCIENT RUINS", L"SETTLEMENTS", L"REALMWEAVE"};
    constexpr std::array subtitles{L"Explore", L"Excavate", L"Discover", L"Build life", L"Transform"};
    constexpr std::array<unsigned, 5> palettes{0x315c45, 0x263d55, 0x665239, 0x704b32, 0x49316d};
    for (std::size_t i = 0; i < layout_.featureCards.size(); ++i) {
        const auto& rect = layout_.featureCards[i];
        fillRect(rect, color(0x09121c, 0.94f), 8);
        strokeRect(rect, color(0x465668, 0.85f), 1, 8);
        fillGradient({rect.x + 2, rect.y + 2, rect.w - 4, 118}, color(palettes[i] + 0x101010), color(palettes[i]));
        blockCluster(rect.x + 10, rect.y + 74, rect.w - 20, 44, color(palettes[i]), static_cast<int>(i) * 17 + 5, 8);
        text(titles[i], {rect.x + 12, rect.y + 125, rect.w - 24, 27}, headingFormat_.Get(), color(0xf1f3f5));
        text(subtitles[i], {rect.x + 12, rect.y + 153, rect.w - 24, 22}, smallFormat_.Get(), color(0x9eb0c4));
        text(i == 0 ? L"FOUNDATION ACTIVE" : L"FUTURE PROGRESSION", {rect.x + 12, rect.y + 178, rect.w - 24, 20},
             tinyFormat_.Get(), i == 0 ? color(0x76dc7d) : color(0x719fd4));
    }
}

void HubPainter::drawNewsBar() {
    fillRect(layout_.newsBar, color(0x08111a, 0.96f), 4);
    text(L"DEVELOPMENT", {205, 868, 125, 26}, tinyFormat_.Get(), color(0xaeb8c4));
    text(L"FRONTIER 0.3", {345, 868, 125, 26}, tinyFormat_.Get(), color(0x48a4ff));
    text(L"The first playable survival foundation is replacing the renderer lab.", {490, 868, 720, 26}, tinyFormat_.Get(), color(0xc4ccd5));
    text(L"v" + buildVersion(), {1470, 868, 90, 26}, tinyFormat_.Get(), color(0x7f9abd), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void HubPainter::draw() {
    createDeviceResources();
    if (!target_) return;
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scaleX(), scaleY()));
    target_->BeginDraw();
    drawBackdrop(); drawScenery(); drawLeftRail(); drawBrand(); drawProfileStrip();
    drawFeatured(); drawStatusPanel(); drawFeatureCards(); drawNewsBar();
    const HRESULT result = target_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) discardDeviceResources();
}

HubAction HubPainter::click(float pixelX, float pixelY) {
    const float x = pixelX / std::max(scaleX(), 0.001f);
    const float y = pixelY / std::max(scaleY(), 0.001f);
    if (layout_.newGameButton.contains(x, y)) return HubAction::NewGame;
    if (layout_.continueButton.contains(x, y) && model_.hasSave()) return HubAction::ContinueGame;
    for (std::size_t i = 0; i < layout_.nav.size(); ++i) {
        if (!layout_.nav[i].contains(x, y)) continue;
        model_.selectNav(i);
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (i == 1) return HubAction::OpenWorlds;
        if (i == 2) return HubAction::OpenSettings;
        if (i == 3) return HubAction::Quit;
        return HubAction::None;
    }
    return HubAction::None;
}

} // namespace rf::ui

#endif
