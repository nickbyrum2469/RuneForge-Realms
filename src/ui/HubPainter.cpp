#ifdef _WIN32

#include "ui/HubPainter.h"

#include "ui/theme/RuneForgePalette.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace rf::ui {
namespace {
using rf::ui::theme::color;

std::wstring buildVersion() {
    std::wstring value;
    for (const char* p = RF_VERSION_STRING; *p; ++p) value.push_back(static_cast<wchar_t>(*p));
    return value;
}

float animatedTime() {
    return static_cast<float>(GetTickCount64() % 600000ULL) / 1000.0f;
}
}

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
    make(L"Bahnschrift", 62.0f, DWRITE_FONT_WEIGHT_BLACK, logoFormat_);
    make(L"Bahnschrift", 34.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, titleFormat_);
    make(L"Bahnschrift", 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, headingFormat_);
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
    pixelWidth_ = std::max(1u, width);
    pixelHeight_ = std::max(1u, height);
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
        D2D1::Point2F(rect.x, rect.y), D2D1::Point2F(rect.x, rect.y + rect.h)),
        collection.Get(), brush.ReleaseAndGetAddressOf());
    target_->FillRectangle(D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
}

void HubPainter::fillRadial(float centerX, float centerY, float radius,
                            D2D1_COLOR_F inner, D2D1_COLOR_F outer) {
    const D2D1_GRADIENT_STOP stops[2]{{0.0f, inner}, {1.0f, outer}};
    ComPtr<ID2D1GradientStopCollection> collection;
    target_->CreateGradientStopCollection(stops, 2, collection.ReleaseAndGetAddressOf());
    ComPtr<ID2D1RadialGradientBrush> brush;
    target_->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(D2D1::Point2F(centerX, centerY), D2D1::Point2F(0, 0), radius, radius),
        collection.Get(), brush.ReleaseAndGetAddressOf());
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radius, radius), brush.Get());
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
            const int variation = (seed * 29 + row * 17 + col * 11) % 13;
            if ((variation == 0 || variation == 12) && row < 3) continue;
            auto tint = base;
            const float delta = (variation - 6) * 0.012f;
            tint.r = std::clamp(tint.r + delta, 0.0f, 1.0f);
            tint.g = std::clamp(tint.g + delta, 0.0f, 1.0f);
            tint.b = std::clamp(tint.b + delta, 0.0f, 1.0f);
            fillRect({x + col * cell, y + row * cell, cell - 0.65f, cell - 0.65f}, tint, 0.8f);
        }
    }
}

void HubPainter::drawGem(float x, float y, float size) {
    fillRect({x, y, size, size}, color(theme::BlueGem, 0.20f), size * 0.30f);
    fillRect({x + size * 0.18f, y + size * 0.18f, size * 0.64f, size * 0.64f}, color(theme::BlueGem, 0.95f), size * 0.20f);
    strokeRect({x - 2, y - 2, size + 4, size + 4}, color(theme::Gold, 0.65f), 1.3f, size * 0.34f);
}

void HubPainter::drawDivider(float x, float y, float width) {
    fillRect({x, y, width, 1.4f}, color(theme::Bronze, 0.55f));
    fillRect({x + width * 0.5f - 44, y - 1, 88, 3.4f}, color(theme::BronzeDark, 0.86f), 2);
    drawGem(x + width * 0.5f - 6, y - 6, 12);
}

void HubPainter::drawPanelFrame(const Rect& rect, bool gold, float radius) {
    fillGradient(rect, color(0x07101a, 0.93f), color(0x04070d, 0.97f));
    strokeRect(rect, color(gold ? theme::Bronze : 0x405269, gold ? 0.92f : 0.70f), gold ? 2.0f : 1.1f, radius);
    strokeRect({rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 10},
               color(gold ? theme::Gold : 0x24374c, gold ? 0.25f : 0.40f), 1.0f, std::max(radius - 3.0f, 1.0f));
}

void HubPainter::drawCastle(float x, float y, float scale, D2D1_COLOR_F stone) {
    fillRect({x, y + 55 * scale, 220 * scale, 95 * scale}, stone, 3);
    fillRect({x + 18 * scale, y + 20 * scale, 46 * scale, 130 * scale}, stone, 3);
    fillRect({x + 152 * scale, y + 10 * scale, 50 * scale, 140 * scale}, stone, 3);
    fillRect({x + 82 * scale, y + 38 * scale, 70 * scale, 112 * scale}, stone, 3);
    fillRect({x + 8 * scale, y + 47 * scale, 210 * scale, 9 * scale}, color(0x302d31, 0.78f), 2);
    const auto light = color(0xffbf5c, 0.90f);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 6; ++col) {
            if ((row + col) % 2 == 0) {
                fillRect({x + (20 + col * 30) * scale, y + (72 + row * 23) * scale,
                          8 * scale, 11 * scale}, light, 2);
            }
        }
    }
}

void HubPainter::drawVoxelTree(float x, float y, float scale, int seed) {
    fillRect({x + 23 * scale, y + 45 * scale, 13 * scale, 60 * scale}, color(0x53331c, 0.96f), 2);
    const std::array offsets{
        std::array<float, 2>{0, 24}, std::array<float, 2>{18, 8}, std::array<float, 2>{36, 22},
        std::array<float, 2>{10, 0}, std::array<float, 2>{28, 0}, std::array<float, 2>{18, 29}
    };
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        const int variation = (seed * 13 + static_cast<int>(i) * 7) % 5;
        fillRect({x + offsets[i][0] * scale, y + offsets[i][1] * scale,
                  (25 + variation * 2) * scale, (25 + variation) * scale},
                 color(0x2d6a32 + static_cast<unsigned>(variation * 0x050600), 0.94f), 4);
    }
}

void HubPainter::drawMountainBand(float baseY, float opacity, D2D1_COLOR_F value, int seed) {
    for (int i = 0; i < 18; ++i) {
        const float center = 150.0f + i * 92.0f;
        const float peak = 65.0f + static_cast<float>((seed * 37 + i * 53) % 115);
        auto mountain = value;
        mountain.a = opacity;
        for (int step = 0; step < 7; ++step) {
            const float width = 148.0f - step * 18.0f;
            fillRect({center - width * 0.5f, baseY - peak + step * 18.0f, width, 22.0f}, mountain, 2);
        }
    }
}

void HubPainter::drawBackdrop() {
    fillGradient({0, 0, 1600, 900}, color(0x06132b), color(0x624044));
    fillRadial(1230, 225, 360, color(0xffb159, 0.31f), color(0x7a3d59, 0.0f));
    fillGradient({170, 385, 1430, 515}, color(0x182a3d, 0.10f), color(0x02050a, 0.98f));
}

void HubPainter::drawScenery() {
    const float time = animatedTime();

    // Slow layered cloud bands give the menu the "breathing" quality of the target reference.
    for (int i = 0; i < 10; ++i) {
        const float cycle = std::fmod(time * (5.0f + (i % 3) * 1.2f) + i * 181.0f, 1680.0f);
        const float x = cycle - 180.0f;
        const float y = 78.0f + (i % 4) * 46.0f;
        fillRect({x, y, 145.0f + (i % 3) * 38.0f, 19.0f + (i % 2) * 7.0f},
                 color(0xe6e0d8, 0.08f + (i % 3) * 0.025f), 14);
    }

    drawMountainBand(395, 0.24f, color(0x3b5572), 13);
    drawMountainBand(470, 0.38f, color(0x273d55), 31);

    // Floating citadel and waterfalls in the upper-left distance.
    blockCluster(218, 110, 330, 88, color(0x253146, 0.92f), 14, 8);
    blockCluster(265, 187, 220, 70, color(0x263b31, 0.92f), 27, 7);
    drawCastle(282, 48, 0.76f, color(0x65616b, 0.94f));
    fillRect({335, 236, 14, 150}, color(0x7cc8ff, 0.16f), 7);
    fillRect({407, 224, 9, 118}, color(0x72b9ff, 0.12f), 5);

    // Distant snowy ridge catching the sunset.
    for (int i = 0; i < 8; ++i) {
        const float w = 440.0f - i * 43.0f;
        fillRect({1025.0f + i * 20.0f, 310.0f - i * 25.0f, w, 31.0f},
                 i > 4 ? color(0xd8e6ed, 0.30f) : color(0x54708a, 0.30f), 3);
    }

    // Valley floor: intentionally much richer than the old rectangular test background.
    blockCluster(370, 425, 870, 250, color(0x294335, 0.74f), 41, 8);
    blockCluster(700, 468, 510, 190, color(0x3d4435, 0.70f), 61, 7);
    drawCastle(755, 350, 1.02f, color(0x625f59, 0.92f));
    drawVoxelTree(545, 442, 1.00f, 9);
    drawVoxelTree(650, 480, 0.78f, 15);
    drawVoxelTree(1065, 432, 1.12f, 21);
    drawVoxelTree(1190, 485, 0.82f, 4);

    // River and reflective highlight.
    fillRect({938, 487, 190, 285}, color(0x173f64, 0.44f), 85);
    fillRect({984, 490, 63, 280}, color(0x6bb6e4, 0.12f), 30);
    fillRect({1012, 505, 14, 230}, color(0xc0e5ff, 0.055f), 7);

    // Far Realmweave portal remains environmental flavor rather than a fake playable mode.
    fillRect({1460, 140, 92, 170}, color(0x25143f, 0.65f), 16);
    strokeRect({1475, 158, 62, 130}, color(0xc44cff, 0.80f), 6.0f, 30);
    strokeRect({1483, 168, 46, 108}, color(0x69a0ff, 0.38f), 2.5f, 24);

    // Foreground vignette gives depth behind the flat UI layer.
    fillRect({170, 690, 1430, 210}, color(0x010308, 0.32f));
    drawVoxelTree(176, 650, 1.55f, 31);
    drawVoxelTree(1500, 625, 1.72f, 18);
}

void HubPainter::drawLeftRail() {
    fillGradient(layout_.leftRail, color(0x03070d, 0.995f), color(0x07111b, 0.995f));
    fillRect({168, 0, 2, 900}, color(theme::BronzeDark, 0.62f));
    constexpr std::array labels{L"HOME", L"WORLDS", L"SETTINGS", L"QUIT"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const bool active = model_.selectedNavIndex() == i;
        if (active) {
            fillGradient(layout_.nav[i], color(0x19293b, 0.98f), color(0x0e1722, 0.98f));
            strokeRect(layout_.nav[i], color(theme::Gold, 0.88f), 1.4f, 8);
            fillRect({layout_.nav[i].x, layout_.nav[i].y + 11, 3, layout_.nav[i].h - 22}, color(theme::GoldBright), 2);
        }
        fillRect({layout_.nav[i].x + 15, layout_.nav[i].y + 23, 18, 18},
                 active ? color(theme::Gold) : color(0x748292), 4);
        text(labels[i], {layout_.nav[i].x + 43, layout_.nav[i].y, 92, layout_.nav[i].h}, headingFormat_.Get(),
             active ? color(theme::GoldBright) : color(0xc1c8d2));
    }

    drawDivider(24, 752, 122);
    fillRect({28, 784, 114, 76}, color(0x08111b, 0.96f), 8);
    strokeRect({28, 784, 114, 76}, color(0x28405b, 0.72f), 1, 8);
    text(L"NATIVE\nVULKAN 1.3", {28, 792, 114, 56}, tinyFormat_.Get(), color(theme::BlueGlow),
         DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawBrand() {
    text(L"RUNEFORGE", {502, 32, 620, 78}, logoFormat_.Get(), color(0x05070a, 0.58f), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"RUNEFORGE", {498, 27, 620, 78}, logoFormat_.Get(), color(theme::Ivory), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"R  E  A  L  M  S", {615, 101, 390, 34}, headingFormat_.Get(), color(theme::Gold),
         DWRITE_TEXT_ALIGNMENT_CENTER);
    fillRect({665, 137, 268, 1}, color(theme::Bronze, 0.52f));
    drawGem(791, 129, 18);
}

void HubPainter::drawProfileStrip() {
    const Rect strip{1020, 18, 550, 76};
    drawPanelFrame(strip, false, 10);
    fillGradient({1037, 30, 52, 52}, color(0x3d9cff), color(0x173d6f));
    strokeRect({1037, 30, 52, 52}, color(theme::BlueGlow, 0.72f), 1.1f, 10);
    text(L"RF", {1037, 30, 52, 52}, headingFormat_.Get(), color(0xffffff), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"Forgekeeper", {1103, 27, 160, 28}, headingFormat_.Get(), color(theme::Ivory));
    text(L"Frontier local session", {1103, 55, 160, 20}, smallFormat_.Get(), color(theme::Emerald));

    fillRect({1280, 31, 126, 42}, color(0x101a27), 7);
    strokeRect({1280, 31, 126, 42}, color(theme::Bronze, 0.36f), 1, 7);
    text(L"FRONTIER", {1280, 31, 126, 42}, smallFormat_.Get(), color(theme::Gold), DWRITE_TEXT_ALIGNMENT_CENTER);

    fillRect({1416, 31, 134, 42}, color(0x101a27), 7);
    strokeRect({1416, 31, 134, 42}, color(0x36516f, 0.50f), 1, 7);
    text(L"BUILD " + buildVersion(), {1416, 31, 134, 42}, tinyFormat_.Get(), color(theme::BlueGlow),
         DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawMiniScene(const Rect& rect, std::size_t index) {
    fillGradient(rect, color(0x172b3f), color(0x192317));
    const unsigned terrain = index == 1 ? 0x263849 : (index == 2 ? 0x5b4935 : (index == 4 ? 0x36264f : 0x31543b));
    blockCluster(rect.x + 4, rect.y + rect.h * 0.54f, rect.w - 8, rect.h * 0.43f,
                 color(terrain, 0.96f), static_cast<int>(index * 23 + 7), 7);

    if (index == 0) {
        drawVoxelTree(rect.x + 20, rect.y + 23, 0.58f, 7);
        drawVoxelTree(rect.x + 155, rect.y + 32, 0.48f, 15);
        fillRect({rect.x + 98, rect.y + 55, 65, 55}, color(0x584e44, 0.82f), 3);
    } else if (index == 1) {
        fillRect({rect.x + 35, rect.y + 30, 185, 78}, color(0x111c28, 0.90f), 30);
        fillRect({rect.x + 78, rect.y + 62, 18, 42}, color(0x49a6ff, 0.35f), 9);
        fillRect({rect.x + 155, rect.y + 49, 12, 55}, color(0x9f61ff, 0.28f), 6);
    } else if (index == 2) {
        drawCastle(rect.x + 55, rect.y + 18, 0.48f, color(0x756759, 0.88f));
    } else if (index == 3) {
        drawCastle(rect.x + 50, rect.y + 28, 0.46f, color(0x6a5d4a, 0.88f));
        drawVoxelTree(rect.x + 10, rect.y + 42, 0.46f, 11);
    } else {
        strokeRect({rect.x + 82, rect.y + 20, 72, 104}, color(0xc552ff, 0.78f), 6, 30);
        strokeRect({rect.x + 93, rect.y + 31, 50, 82}, color(0x629cff, 0.42f), 3, 23);
    }
}

void HubPainter::drawFeatured() {
    drawPanelFrame(layout_.featured, true, 12);
    fillGradient({layout_.featured.x + 2, layout_.featured.y + 2, 485, layout_.featured.h - 4},
                 color(0x050910, 0.96f), color(0x0b1118, 0.93f));

    text(L"FLAGSHIP SURVIVAL REALM", {220, 251, 315, 24}, smallFormat_.Get(), color(theme::BlueGlow));
    text(L"Frontier Realms", {220, 284, 465, 54}, titleFormat_.Get(), color(theme::Ivory));
    text(L"Persistent procedural survival", {220, 342, 420, 25}, bodyFormat_.Get(), color(theme::Gold));
    text(L"Wake in a living voxel frontier. Explore, gather, carve and build from individual blocks toward structures, settlements and eventually the land itself.",
         {220, 380, 430, 88}, bodyFormat_.Get(), color(0xdfe4e9));

    constexpr std::array<const wchar_t*, 3> tags{L"SURVIVAL", L"PROCEDURAL", L"PERSISTENT"};
    for (std::size_t i = 0; i < tags.size(); ++i) {
        const Rect tag{220.0f + static_cast<float>(i) * 116.0f, 468, 106, 24};
        fillRect(tag, color(i == 0 ? 0x112a3d : (i == 1 ? 0x1d2635 : 0x1e3324), 0.96f), 5);
        strokeRect(tag, color(i == 2 ? theme::Emerald : theme::BlueGem, 0.35f), 1, 5);
        text(tags[i], tag, tinyFormat_.Get(), color(i == 2 ? 0xa6e69f : 0x8ecbff), DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    const Rect heroScene{690, 252, 398, 318};
    fillGradient(heroScene, color(0x173253), color(0x1d311f));
    fillRadial(1000, 310, 165, color(0xffb85a, 0.25f), color(0x1d314a, 0.0f));
    drawMountainBand(408, 0.30f, color(0x49657a), 41);
    blockCluster(704, 401, 368, 157, color(0x38593b, 0.94f), 71, 7);
    drawCastle(785, 326, 0.80f, color(0x6c6657, 0.94f));
    drawVoxelTree(728, 405, 0.62f, 5);
    drawVoxelTree(1000, 412, 0.66f, 17);
    fillRect({917, 442, 72, 117}, color(0x1f5b83, 0.42f), 35);
    strokeRect(heroScene, color(theme::Gold, 0.50f), 1.2f, 9);

    const bool canContinue = model_.hasSave();
    fillGradient(layout_.continueButton,
                 canContinue ? color(0x183653) : color(0x111821),
                 canContinue ? color(0x102238) : color(0x0b1017));
    strokeRect(layout_.continueButton, canContinue ? color(0x69b7ef) : color(0x3a4652), 1.4f, 8);
    text(canContinue ? L"CONTINUE" : L"NO SAVE YET", layout_.continueButton, headingFormat_.Get(),
         canContinue ? color(0xe0f4ff) : color(0x6f7b88), DWRITE_TEXT_ALIGNMENT_CENTER);

    fillGradient(layout_.newGameButton, color(0xffd56e), color(0xc98a28));
    strokeRect(layout_.newGameButton, color(0xffefb8), 1.8f, 8);
    text(L"NEW FRONTIER", layout_.newGameButton, headingFormat_.Get(), color(0x261806), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void HubPainter::drawStatusPanel() {
    drawPanelFrame(layout_.statusPanel, false, 11);
    text(L"PARTY & REALM", {1160, 248, 220, 28}, headingFormat_.Get(), color(theme::Ivory));
    text(L"LOCAL", {1430, 250, 100, 24}, tinyFormat_.Get(), color(theme::Emerald), DWRITE_TEXT_ALIGNMENT_TRAILING);
    drawDivider(1160, 286, 390);

    const std::array<const wchar_t*, 4> names{L"Forgekeeper", L"Open party slot", L"Open party slot", L"Open party slot"};
    for (int i = 0; i < 4; ++i) {
        const float y = 307.0f + i * 54.0f;
        fillRect({1160, y, 390, 45}, color(i == 0 ? 0x101f2c : 0x0b131d, 0.92f), 7);
        fillRect({1172, y + 8, 29, 29}, color(i == 0 ? 0x2f8ed4 : 0x172331), 7);
        if (i == 0) text(L"RF", {1172, y + 8, 29, 29}, tinyFormat_.Get(), color(0xffffff), DWRITE_TEXT_ALIGNMENT_CENTER);
        text(names[i], {1212, y, 210, 45}, smallFormat_.Get(), color(i == 0 ? theme::Ivory : theme::Muted));
        text(i == 0 ? L"ONLINE" : L"FUTURE CO-OP", {1415, y, 120, 45}, tinyFormat_.Get(),
             color(i == 0 ? theme::Emerald : 0x61768c), DWRITE_TEXT_ALIGNMENT_TRAILING);
    }

    fillRect({1160, 535, 390, 48}, color(0x0b151f, 0.95f), 7);
    text(L"VISUAL FOUNDATION", {1174, 535, 190, 48}, tinyFormat_.Get(), color(theme::BlueGlow));
    text(L"0.4.0", {1420, 535, 110, 48}, headingFormat_.Get(), color(theme::Gold), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void HubPainter::drawFeatureCards() {
    text(L"THE FRONTIER GROWS WITH YOU", {195, 612, 330, 28}, smallFormat_.Get(), color(0xd7dce2));
    constexpr std::array titles{L"WILDLANDS", L"DEEP CAVERNS", L"ANCIENT RUINS", L"SETTLEMENTS", L"REALMWEAVE"};
    constexpr std::array subtitles{L"Explore", L"Excavate", L"Discover", L"Build life", L"Transform"};

    for (std::size_t i = 0; i < layout_.featureCards.size(); ++i) {
        const auto& rect = layout_.featureCards[i];
        drawPanelFrame(rect, i == 0, 9);
        const Rect scene{rect.x + 4, rect.y + 4, rect.w - 8, 113};
        drawMiniScene(scene, i);
        text(titles[i], {rect.x + 13, rect.y + 124, rect.w - 26, 28}, headingFormat_.Get(), color(theme::Ivory));
        text(subtitles[i], {rect.x + 13, rect.y + 153, rect.w - 26, 21}, smallFormat_.Get(), color(theme::Muted));
        text(i == 0 ? L"ACTIVE FRONTIER" : L"FUTURE PROGRESSION",
             {rect.x + 13, rect.y + 178, rect.w - 26, 20}, tinyFormat_.Get(),
             i == 0 ? color(theme::Emerald) : color(0x76a6d8));
    }
}

void HubPainter::drawNewsBar() {
    fillRect(layout_.newsBar, color(0x050b12, 0.985f), 4);
    fillRect({layout_.newsBar.x, layout_.newsBar.y, 4, layout_.newsBar.h}, color(theme::Gold), 2);
    text(L"DEVELOPMENT", {207, 868, 125, 26}, tinyFormat_.Get(), color(0xaeb8c4));
    text(L"VISUAL 0.4", {345, 868, 125, 26}, tinyFormat_.Get(), color(theme::BlueGlow));
    text(L"Hero materials, procedural sky, forged HUD and the first native inventory presentation are entering Frontier.",
         {490, 868, 820, 26}, tinyFormat_.Get(), color(0xcbd3db));
    text(L"v" + buildVersion(), {1470, 868, 90, 26}, tinyFormat_.Get(), color(0x8ca6c3), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void HubPainter::draw() {
    createDeviceResources();
    if (!target_) return;

    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scaleX(), scaleY()));
    drawBackdrop();
    drawScenery();
    drawLeftRail();
    drawBrand();
    drawProfileStrip();
    drawFeatured();
    drawStatusPanel();
    drawFeatureCards();
    drawNewsBar();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());

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
