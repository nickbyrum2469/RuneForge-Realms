#ifdef _WIN32

#include "ui/inventory/InventoryPainter.h"

#include "ui/theme/RuneForgePalette.h"

#include <algorithm>
#include <array>

namespace rf::ui::inventory {
namespace {
using rf::ui::theme::color;

constexpr std::array<const wchar_t*, 6> equipmentLabels{
    L"HEAD", L"CHEST", L"HANDS", L"RELIC", L"LEGS", L"BOOTS"
};

struct ItemVisual {
    unsigned top;
    unsigned side;
    unsigned edge;
    const wchar_t* name;
};

constexpr std::array<ItemVisual, 5> starterItems{{
    {0x65ad31, 0x3d7625, 0x9ad75a, L"Turf"},
    {0x80502a, 0x52311d, 0xb57942, L"Soil"},
    {0x8e9295, 0x5c6268, 0xb9bec1, L"Stone"},
    {0xa45f28, 0x633817, 0xd08a43, L"Oak"},
    {0x3f8c34, 0x245825, 0x68bd55, L"Leaves"},
}};
}

InventoryPainter::InventoryPainter(HWND hwnd) : hwnd_(hwnd) {}
InventoryPainter::~InventoryPainter() = default;

bool InventoryPainter::initialize() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(dwriteFactory_.ReleaseAndGetAddressOf())))) return false;
    createTextResources();
    createDeviceResources();
    return target_ != nullptr;
}

void InventoryPainter::createTextResources() {
    auto make = [this](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                       ComPtr<IDWriteTextFormat>& output) {
        dwriteFactory_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, size, L"en-us",
                                         output.ReleaseAndGetAddressOf());
        if (output) {
            output->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            output->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    };
    make(L"Bahnschrift", 38.0f, DWRITE_FONT_WEIGHT_BLACK, titleFormat_);
    make(L"Bahnschrift", 20.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, headingFormat_);
    make(L"Segoe UI", 14.0f, DWRITE_FONT_WEIGHT_NORMAL, bodyFormat_);
    make(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, smallFormat_);
    make(L"Segoe UI", 10.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, tinyFormat_);
}

void InventoryPainter::createDeviceResources() {
    if (target_) return;
    RECT rect{};
    GetClientRect(hwnd_, &rect);
    pixelWidth_ = static_cast<unsigned>(std::max(1L, rect.right - rect.left));
    pixelHeight_ = static_cast<unsigned>(std::max(1L, rect.bottom - rect.top));
    d2dFactory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(pixelWidth_, pixelHeight_)),
        target_.ReleaseAndGetAddressOf());
}

void InventoryPainter::discardDeviceResources() { target_.Reset(); }

void InventoryPainter::resize(unsigned width, unsigned height) {
    pixelWidth_ = std::max(width, 1u);
    pixelHeight_ = std::max(height, 1u);
    if (target_) target_->Resize(D2D1::SizeU(pixelWidth_, pixelHeight_));
}

void InventoryPainter::fillRect(const Rect& rect, D2D1_COLOR_F value, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(value, brush.ReleaseAndGetAddressOf());
    const auto d2d = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0.0f) target_->FillRoundedRectangle(D2D1::RoundedRect(d2d, radius, radius), brush.Get());
    else target_->FillRectangle(d2d, brush.Get());
}

void InventoryPainter::strokeRect(const Rect& rect, D2D1_COLOR_F value, float thickness, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(value, brush.ReleaseAndGetAddressOf());
    const auto d2d = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0.0f) target_->DrawRoundedRectangle(D2D1::RoundedRect(d2d, radius, radius), brush.Get(), thickness);
    else target_->DrawRectangle(d2d, brush.Get(), thickness);
}

void InventoryPainter::fillGradient(const Rect& rect, D2D1_COLOR_F top, D2D1_COLOR_F bottom) {
    const D2D1_GRADIENT_STOP stops[2]{{0.0f, top}, {1.0f, bottom}};
    ComPtr<ID2D1GradientStopCollection> collection;
    target_->CreateGradientStopCollection(stops, 2, collection.ReleaseAndGetAddressOf());
    ComPtr<ID2D1LinearGradientBrush> brush;
    target_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.x, rect.y), D2D1::Point2F(rect.x, rect.y + rect.h)),
        collection.Get(), brush.ReleaseAndGetAddressOf());
    target_->FillRectangle(D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
}

void InventoryPainter::text(std::wstring_view value, const Rect& rect, IDWriteTextFormat* format,
                            D2D1_COLOR_F valueColor, DWRITE_TEXT_ALIGNMENT alignment) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(valueColor, brush.ReleaseAndGetAddressOf());
    format->SetTextAlignment(alignment);
    target_->DrawTextW(value.data(), static_cast<UINT32>(value.size()), format,
                       D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
}

void InventoryPainter::drawGem(float x, float y, float size) {
    fillRect({x, y, size, size}, color(theme::BlueGem, 0.30f), size * 0.25f);
    fillRect({x + size * 0.18f, y + size * 0.18f, size * 0.64f, size * 0.64f},
             color(theme::BlueGem, 0.95f), size * 0.18f);
    strokeRect({x - 2, y - 2, size + 4, size + 4}, color(theme::Gold, 0.65f), 1.4f, size * 0.30f);
}

void InventoryPainter::drawDivider(float x, float y, float width) {
    fillRect({x, y, width, 2}, color(theme::Bronze, 0.70f));
    fillRect({x + width * 0.5f - 38, y - 1, 76, 4}, color(theme::BronzeDark, 0.90f), 2);
    drawGem(x + width * 0.5f - 7, y - 7, 14);
}

void InventoryPainter::drawBackdrop() {
    fillGradient(layout_.backdrop, color(0x050812), color(0x1d1515));
    fillRect({0, 0, 1600, 900}, color(0x02040a, 0.34f));

    // Faint forged-stone bands keep the screen from reading as flat black.
    for (int i = 0; i < 18; ++i) {
        const float y = 30.0f + i * 52.0f;
        fillRect({0, y, 1600, 1.0f}, color(0xa67a3b, (i % 3 == 0) ? 0.055f : 0.025f));
    }
}

void InventoryPainter::drawOuterFrame() {
    fillRect(layout_.outerFrame, color(theme::Panel, 0.985f), 14);
    strokeRect(layout_.outerFrame, color(theme::BronzeDark, 0.95f), 8.0f, 14);
    strokeRect({layout_.outerFrame.x + 8, layout_.outerFrame.y + 8,
                layout_.outerFrame.w - 16, layout_.outerFrame.h - 16},
               color(theme::Gold, 0.78f), 1.7f, 10);
    strokeRect({layout_.outerFrame.x + 16, layout_.outerFrame.y + 16,
                layout_.outerFrame.w - 32, layout_.outerFrame.h - 32},
               color(0x30435b, 0.65f), 1.0f, 7);

    fillGradient(layout_.titlePlate, color(0x221b12), color(0x0d1018));
    strokeRect(layout_.titlePlate, color(theme::Gold, 0.94f), 2.0f, 11);
    text(L"FORGEKEEPER'S PACK", layout_.titlePlate, titleFormat_.Get(), color(theme::Ivory),
         DWRITE_TEXT_ALIGNMENT_CENTER);
    drawGem(layout_.titlePlate.x - 24, layout_.titlePlate.y + 20, 20);
    drawGem(layout_.titlePlate.x + layout_.titlePlate.w + 4, layout_.titlePlate.y + 20, 20);

    text(L"E / ESC  RETURN TO FRONTIER", {1170, 84, 230, 24}, tinyFormat_.Get(),
         color(theme::Muted), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void InventoryPainter::drawSlot(const Rect& rect, bool selected) {
    const auto border = selected ? color(theme::GoldBright, 0.98f) : color(theme::Bronze, 0.58f);
    fillGradient(rect, color(0x050911, 0.98f), color(0x111823, 0.98f));
    strokeRect(rect, color(0x02040a), 5.0f, 6);
    strokeRect({rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6}, border, selected ? 2.4f : 1.2f, 5);
    fillRect({rect.x + 9, rect.y + 9, rect.w - 18, 2}, color(0xffffff, 0.045f), 1);
}

void InventoryPainter::drawItemCube(const Rect& rect, unsigned top, unsigned side, unsigned edge) {
    const float size = std::min(rect.w, rect.h) * 0.48f;
    const float x = rect.x + (rect.w - size) * 0.5f;
    const float y = rect.y + (rect.h - size) * 0.47f;
    fillRect({x + size * 0.10f, y + size * 0.14f, size * 0.82f, size * 0.78f}, color(side), 4);
    fillRect({x + size * 0.16f, y, size * 0.70f, size * 0.30f}, color(top), 4);
    strokeRect({x + size * 0.10f, y, size * 0.82f, size * 0.92f}, color(edge, 0.78f), 1.2f, 4);
    fillRect({x + size * 0.19f, y + size * 0.07f, size * 0.45f, 2.0f}, color(0xffffff, 0.16f), 1);
}

void InventoryPainter::drawEquipment() {
    fillGradient(layout_.equipmentPanel, color(0x0b1018, 0.98f), color(0x080b11, 0.98f));
    strokeRect(layout_.equipmentPanel, color(theme::Bronze, 0.68f), 1.5f, 10);
    text(L"EQUIPMENT", {215, 165, 415, 38}, headingFormat_.Get(), color(theme::GoldBright),
         DWRITE_TEXT_ALIGNMENT_CENTER);
    drawDivider(235, 205, 375);

    fillGradient(layout_.characterViewport, color(0x0d1722), color(0x06090e));
    strokeRect(layout_.characterViewport, color(0x37506d, 0.70f), 1.1f, 9);

    // Stylized paper-doll silhouette; equipment rendering can replace this later.
    fillRect({397, 252, 58, 58}, color(0x1f3346), 20);
    fillGradient({376, 318, 100, 148}, color(0x263d51), color(0x142535));
    fillRect({350, 330, 28, 132}, color(0x1a2d3e), 10);
    fillRect({474, 330, 28, 132}, color(0x1a2d3e), 10);
    fillRect({389, 460, 34, 120}, color(0x17293a), 10);
    fillRect({430, 460, 34, 120}, color(0x17293a), 10);
    fillRect({367, 584, 66, 13}, color(theme::BlueGem, 0.15f), 7);
    fillRect({426, 584, 66, 13}, color(theme::BlueGem, 0.15f), 7);

    for (std::size_t i = 0; i < layout_.equipmentSlots.size(); ++i) {
        drawSlot(layout_.equipmentSlots[i]);
        text(equipmentLabels[i], {layout_.equipmentSlots[i].x, layout_.equipmentSlots[i].y + 80,
                                  layout_.equipmentSlots[i].w, 18}, tinyFormat_.Get(), color(theme::Muted),
             DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    fillRect({240, 650, 365, 74}, color(0x0c131d), 8);
    strokeRect({240, 650, 365, 74}, color(0x355679, 0.68f), 1.0f, 8);
    text(L"FORGEKEEPER", {258, 660, 190, 24}, headingFormat_.Get(), color(theme::Ivory));
    text(L"Frontier wanderer  •  No armor equipped", {258, 687, 318, 20}, smallFormat_.Get(), color(theme::Muted));
}

void InventoryPainter::drawQuickCraft() {
    fillRect(layout_.quickCraftPanel, color(0x080d14, 0.98f), 9);
    strokeRect(layout_.quickCraftPanel, color(theme::Bronze, 0.58f), 1.1f, 9);
    text(L"QUICK CRAFT", {1028, 216, 312, 28}, headingFormat_.Get(), color(theme::GoldBright),
         DWRITE_TEXT_ALIGNMENT_CENTER);

    for (std::size_t i = 0; i < layout_.craftSlots.size(); ++i) {
        drawSlot(layout_.craftSlots[i], i == layout_.craftSlots.size() - 1);
    }
    drawItemCube(layout_.craftSlots[0], starterItems[3].top, starterItems[3].side, starterItems[3].edge);
    drawItemCube(layout_.craftSlots[1], starterItems[3].top, starterItems[3].side, starterItems[3].edge);
    fillRect({1258, 277, 12, 4}, color(theme::Gold, 0.80f), 2);
    drawItemCube(layout_.craftSlots[3], 0xc69751, 0x78502b, 0xf0c878);
    text(L"Prototype recipe preview", {1030, 329, 310, 24}, smallFormat_.Get(), color(theme::Muted),
         DWRITE_TEXT_ALIGNMENT_CENTER);
}

void InventoryPainter::drawInventoryGrid() {
    fillGradient(layout_.inventoryPanel, color(0x0b1017, 0.99f), color(0x070a0f, 0.99f));
    strokeRect(layout_.inventoryPanel, color(theme::Bronze, 0.68f), 1.5f, 10);
    text(L"INVENTORY", {735, 165, 640, 38}, headingFormat_.Get(), color(theme::GoldBright),
         DWRITE_TEXT_ALIGNMENT_CENTER);
    drawDivider(765, 205, 580);
    text(L"SURVIVAL LOADOUT", {750, 224, 210, 24}, smallFormat_.Get(), color(theme::BlueGlow));
    text(L"The data model arrives with the Survival Slice; this screen establishes the final visual language.",
         {750, 247, 580, 44}, bodyFormat_.Get(), color(theme::Muted));

    drawQuickCraft();

    for (std::size_t i = 0; i < layout_.inventorySlots.size(); ++i) {
        drawSlot(layout_.inventorySlots[i]);
        if (i < starterItems.size()) {
            const auto& item = starterItems[i];
            drawItemCube(layout_.inventorySlots[i], item.top, item.side, item.edge);
            text(item.name, {layout_.inventorySlots[i].x + 4, layout_.inventorySlots[i].y + 57,
                             layout_.inventorySlots[i].w - 8, 16}, tinyFormat_.Get(), color(theme::Ivory),
                 DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    }
}

void InventoryPainter::drawHotbar() {
    fillRect(layout_.hotbarPanel, color(0x05080e, 0.96f), 10);
    strokeRect(layout_.hotbarPanel, color(theme::BronzeDark, 0.92f), 2.0f, 10);
    for (std::size_t i = 0; i < layout_.hotbarSlots.size(); ++i) {
        drawSlot(layout_.hotbarSlots[i], i == 1);
        if (i < starterItems.size()) {
            const auto& item = starterItems[i];
            drawItemCube(layout_.hotbarSlots[i], item.top, item.side, item.edge);
        }
        text(std::to_wstring(i + 1), {layout_.hotbarSlots[i].x + 4, layout_.hotbarSlots[i].y + 2, 14, 14},
             tinyFormat_.Get(), color(theme::Muted), DWRITE_TEXT_ALIGNMENT_CENTER);
    }
}

void InventoryPainter::draw() {
    createDeviceResources();
    if (!target_) return;

    const float sx = static_cast<float>(pixelWidth_) / InventoryLayout::width;
    const float sy = static_cast<float>(pixelHeight_) / InventoryLayout::height;
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy));
    target_->Clear(color(theme::Void));

    drawBackdrop();
    drawOuterFrame();
    drawEquipment();
    drawInventoryGrid();
    drawHotbar();

    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    const HRESULT result = target_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) discardDeviceResources();
}

} // namespace rf::ui::inventory

#endif
