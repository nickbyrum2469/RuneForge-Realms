#ifdef _WIN32

#include "ui/inventory/InventoryPainter.h"

#include "game/items/ItemId.h"
#include "ui/theme/RuneForgePalette.h"

#include <algorithm>
#include <array>
#include <string>

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

ItemVisual visualFor(game::items::ItemId item) noexcept {
    switch (item) {
        case game::items::ItemId::GrassBlock: return {0x65ad31, 0x3d7625, 0x9ad75a, L"Turf"};
        case game::items::ItemId::DirtBlock: return {0x80502a, 0x52311d, 0xb57942, L"Soil"};
        case game::items::ItemId::StoneBlock: return {0x8e9295, 0x5c6268, 0xb9bec1, L"Stone"};
        case game::items::ItemId::OakLog: return {0xa45f28, 0x633817, 0xd08a43, L"Oak"};
        case game::items::ItemId::Leaves: return {0x3f8c34, 0x245825, 0x68bd55, L"Leaves"};
        case game::items::ItemId::None: break;
    }
    return {0x313942, 0x1a2129, 0x52606d, L"Empty"};
}

std::wstring wide(std::string_view value) { return std::wstring(value.begin(), value.end()); }
}

InventoryPainter::InventoryPainter(HWND hwnd) : hwnd_(hwnd) {}
InventoryPainter::~InventoryPainter() = default;

void InventoryPainter::setInventory(const game::inventory::Inventory& inventory,
                                    game::mining::MiningMode miningMode) noexcept {
    inventory_ = inventory;
    miningMode_ = miningMode;
}

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
    make(L"Georgia", 38.0f, DWRITE_FONT_WEIGHT_BOLD, titleFormat_);
    make(L"Georgia", 20.0f, DWRITE_FONT_WEIGHT_BOLD, headingFormat_);
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
    if (!format) return;
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(valueColor, brush.ReleaseAndGetAddressOf());
    format->SetTextAlignment(alignment);
    target_->DrawTextW(value.data(), static_cast<UINT32>(value.size()), format,
                       D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
}

void InventoryPainter::drawGem(float x, float y, float size) {
    const float cx = x + size * 0.5f;
    const float cy = y + size * 0.5f;
    const float r = size * 0.5f;
    ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(d2dFactory_->CreatePathGeometry(geometry.ReleaseAndGetAddressOf())) || !geometry) return;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.ReleaseAndGetAddressOf())) || !sink) return;
    sink->BeginFigure(D2D1::Point2F(cx, cy - r), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(cx + r, cy));
    sink->AddLine(D2D1::Point2F(cx, cy + r));
    sink->AddLine(D2D1::Point2F(cx - r, cy));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> edgeBrush;
    target_->CreateSolidColorBrush(color(theme::BlueGem), fillBrush.ReleaseAndGetAddressOf());
    target_->CreateSolidColorBrush(color(theme::GoldBright), edgeBrush.ReleaseAndGetAddressOf());
    target_->FillGeometry(geometry.Get(), fillBrush.Get());
    target_->DrawGeometry(geometry.Get(), edgeBrush.Get(), 1.5f);
    fillRect({cx - r * 0.18f, cy - r * 0.42f, r * 0.34f, r * 0.34f}, color(theme::BlueCore, 0.86f), 1.0f);
}

void InventoryPainter::drawDivider(float x, float y, float width) {
    fillRect({x, y, width, 4}, color(theme::BronzeDeep, 0.96f));
    fillRect({x, y, width, 1.2f}, color(theme::Gold, 0.82f));
    drawGem(x + width * 0.5f - 6, y - 5, 12);
}

void InventoryPainter::drawBackdrop() {
    fillGradient(layout_.backdrop, color(0x080b10), color(0x030406));
    // Quiet silhouettes keep the popup from reading like a blank developer window while leaving
    // inventory information dominant, echoing the old cinematic RuneForge sunset-menu reference.
    for (int i = 0; i < 11; ++i) {
        const float x = -30.0f + static_cast<float>(i) * 155.0f;
        const float h = 70.0f + static_cast<float>((i * 53) % 120);
        fillRect({x, 900.0f - h, 120.0f, h}, color(0x0b1016, 0.74f));
    }
    fillRect({0, 0, 1600, 900}, color(0x020308, 0.24f));
}

void InventoryPainter::drawOuterFrame() {
    fillRect(layout_.outerFrame, color(theme::Panel, 0.995f), 7);
    strokeRect(layout_.outerFrame, color(theme::BronzeDeep), 9.0f, 7);
    strokeRect({layout_.outerFrame.x + 8, layout_.outerFrame.y + 8,
                layout_.outerFrame.w - 16, layout_.outerFrame.h - 16},
               color(theme::Bronze), 2.3f, 5);
    strokeRect({layout_.outerFrame.x + 14, layout_.outerFrame.y + 14,
                layout_.outerFrame.w - 28, layout_.outerFrame.h - 28},
               color(theme::Gold, 0.48f), 0.9f, 3);

    fillGradient(layout_.titlePlate, color(0x24180d), color(theme::PanelRaised));
    strokeRect(layout_.titlePlate, color(theme::BronzeDeep), 6.0f, 5);
    strokeRect({layout_.titlePlate.x + 5, layout_.titlePlate.y + 5,
                layout_.titlePlate.w - 10, layout_.titlePlate.h - 10},
               color(theme::Gold, 0.90f), 1.6f, 3);
    text(L"FORGEKEEPER'S PACK", {layout_.titlePlate.x, layout_.titlePlate.y + 2,
                                  layout_.titlePlate.w, 53}, titleFormat_.Get(),
         color(theme::GoldBright), DWRITE_TEXT_ALIGNMENT_CENTER);
    text(L"FRONTIER INVENTORY", {layout_.titlePlate.x, layout_.titlePlate.y + 54,
                                   layout_.titlePlate.w, 22}, tinyFormat_.Get(),
         color(theme::BlueGlow), DWRITE_TEXT_ALIGNMENT_CENTER);
    drawGem(layout_.titlePlate.x - 10, layout_.titlePlate.y + 31, 20);
    drawGem(layout_.titlePlate.x + layout_.titlePlate.w - 10, layout_.titlePlate.y + 31, 20);
    text(L"I / ESC  RETURN TO FRONTIER", {1180, 80, 235, 24}, tinyFormat_.Get(),
         color(theme::Muted), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void InventoryPainter::drawSlot(const Rect& rect, bool selected) {
    const auto border = selected ? color(theme::GoldBright, 0.98f) : color(theme::Bronze, 0.66f);
    fillGradient(rect, color(theme::PanelInset), color(0x111318));
    strokeRect(rect, color(theme::BronzeDeep), 4.0f, 3);
    strokeRect({rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8}, border,
               selected ? 2.2f : 1.0f, 2);
    fillRect({rect.x + 8, rect.y + 8, rect.w - 16, 1.5f}, color(0xffffff, 0.045f));
}

void InventoryPainter::drawItemCube(const Rect& rect, unsigned top, unsigned side, unsigned edge) {
    const float size = std::min(rect.w, rect.h) * 0.48f;
    const float x = rect.x + (rect.w - size) * 0.5f;
    const float y = rect.y + (rect.h - size) * 0.43f;
    fillRect({x + size * 0.10f, y + size * 0.14f, size * 0.82f, size * 0.78f}, color(side), 3);
    fillRect({x + size * 0.16f, y, size * 0.70f, size * 0.30f}, color(top), 3);
    strokeRect({x + size * 0.10f, y, size * 0.82f, size * 0.92f}, color(edge, 0.78f), 1.2f, 3);
}

void InventoryPainter::drawStack(const Rect& rect, const game::inventory::ItemStack& stack) {
    if (stack.empty()) return;
    const auto visual = visualFor(stack.item);
    drawItemCube(rect, visual.top, visual.side, visual.edge);
    text(std::to_wstring(stack.count), {rect.x + rect.w - 31, rect.y + rect.h - 23, 25, 18},
         tinyFormat_.Get(), color(theme::Ivory), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void InventoryPainter::drawEquipment() {
    fillGradient(layout_.equipmentPanel, color(theme::PanelRaised, 0.98f), color(theme::PanelInset, 0.99f));
    strokeRect(layout_.equipmentPanel, color(theme::BronzeDeep), 5.0f, 5);
    strokeRect({layout_.equipmentPanel.x + 6, layout_.equipmentPanel.y + 6,
                layout_.equipmentPanel.w - 12, layout_.equipmentPanel.h - 12},
               color(theme::Gold, 0.56f), 1.0f, 3);
    text(L"EQUIPMENT", {195, 184, 420, 34}, headingFormat_.Get(), color(theme::GoldBright), DWRITE_TEXT_ALIGNMENT_CENTER);
    drawDivider(215, 223, 380);

    fillGradient(layout_.characterViewport, color(0x11161d), color(0x07090d));
    strokeRect(layout_.characterViewport, color(theme::Bronze, 0.55f), 1.4f, 4);
    // Simple paper-doll silhouette remains a preview placeholder; the framing now makes that status
    // intentional instead of pretending to be the finished 3D avatar preview.
    fillRect({397, 272, 52, 52}, color(0x24303a), 12);
    fillGradient({379, 326, 88, 120}, color(0x293846), color(0x17212b));
    fillRect({355, 338, 25, 108}, color(0x1e2a34), 7);
    fillRect({466, 338, 25, 108}, color(0x1e2a34), 7);
    fillRect({390, 440, 30, 112}, color(0x1b2731), 7);
    fillRect({427, 440, 30, 112}, color(0x1b2731), 7);
    text(L"GEAR PREVIEW", {345, 552, 155, 18}, tinyFormat_.Get(), color(theme::Muted), DWRITE_TEXT_ALIGNMENT_CENTER);

    for (std::size_t i = 0; i < layout_.equipmentSlots.size(); ++i) {
        drawSlot(layout_.equipmentSlots[i]);
        text(equipmentLabels[i], {layout_.equipmentSlots[i].x, layout_.equipmentSlots[i].y + 78,
                                  layout_.equipmentSlots[i].w, 18}, tinyFormat_.Get(), color(theme::Muted),
             DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    fillRect({205, 625, 400, 92}, color(theme::PanelInset), 4);
    strokeRect({205, 625, 400, 92}, color(theme::Bronze, 0.62f), 1.2f, 4);
    drawGem(220, 651, 20);
    text(L"FORGEKEEPER", {252, 638, 195, 28}, headingFormat_.Get(), color(theme::Ivory));
    text(L"Frontier wanderer  |  Equipment system forming",
         {252, 672, 325, 22}, smallFormat_.Get(), color(theme::Muted));
}

void InventoryPainter::drawQuickCraft() {
    fillGradient(layout_.quickCraftPanel, color(0x121318, 0.99f), color(theme::PanelInset, 0.99f));
    strokeRect(layout_.quickCraftPanel, color(theme::BronzeDeep), 4.0f, 4);
    strokeRect({layout_.quickCraftPanel.x + 5, layout_.quickCraftPanel.y + 5,
                layout_.quickCraftPanel.w - 10, layout_.quickCraftPanel.h - 10},
               color(theme::Gold, 0.48f), 0.9f, 2);
    text(L"QUICK CRAFT", {1038, 248, 335, 26}, headingFormat_.Get(), color(theme::GoldBright), DWRITE_TEXT_ALIGNMENT_CENTER);
    for (std::size_t i = 0; i < layout_.craftSlots.size(); ++i) drawSlot(layout_.craftSlots[i], false);
    text(L"RECIPES ARRIVE WITH SURVIVAL CRAFTING", {1040, 350, 330, 18}, tinyFormat_.Get(),
         color(theme::Muted), DWRITE_TEXT_ALIGNMENT_CENTER);
}

void InventoryPainter::drawInventoryGrid() {
    fillGradient(layout_.inventoryPanel, color(theme::PanelRaised, 0.99f), color(theme::PanelInset, 0.99f));
    strokeRect(layout_.inventoryPanel, color(theme::BronzeDeep), 5.0f, 5);
    strokeRect({layout_.inventoryPanel.x + 6, layout_.inventoryPanel.y + 6,
                layout_.inventoryPanel.w - 12, layout_.inventoryPanel.h - 12},
               color(theme::Gold, 0.56f), 1.0f, 3);
    text(L"INVENTORY", {715, 184, 680, 34}, headingFormat_.Get(), color(theme::GoldBright), DWRITE_TEXT_ALIGNMENT_CENTER);
    drawDivider(735, 223, 640);

    const std::wstring mode = wide(game::mining::miningModeName(miningMode_));
    text(L"MINING MODE  |  " + mode, {725, 245, 285, 22}, smallFormat_.Get(), color(theme::BlueGlow));
    text(L"Collected blocks are real stacks. Break terrain, recover drops, then build from the selected hotbar slot.",
         {725, 271, 280, 58}, bodyFormat_.Get(), color(theme::Muted));
    drawQuickCraft();

    for (std::size_t i = 0; i < layout_.inventorySlots.size(); ++i) {
        drawSlot(layout_.inventorySlots[i]);
        if (i >= game::inventory::Inventory::backpackSize) continue;
        const auto& stack = inventory_.slot(game::inventory::Inventory::hotbarSize + i);
        drawStack(layout_.inventorySlots[i], stack);
        if (!stack.empty()) {
            const auto visual = visualFor(stack.item);
            text(visual.name, {layout_.inventorySlots[i].x + 4, layout_.inventorySlots[i].y + 55,
                              layout_.inventorySlots[i].w - 8, 16}, tinyFormat_.Get(), color(theme::Ivory),
                 DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    }
}

void InventoryPainter::drawHotbar() {
    fillRect(layout_.hotbarPanel, color(theme::Panel, 0.995f), 5);
    strokeRect(layout_.hotbarPanel, color(theme::BronzeDeep), 5.0f, 5);
    strokeRect({layout_.hotbarPanel.x + 5, layout_.hotbarPanel.y + 5,
                layout_.hotbarPanel.w - 10, layout_.hotbarPanel.h - 10},
               color(theme::Gold, 0.50f), 0.9f, 3);
    drawGem(layout_.hotbarPanel.x + layout_.hotbarPanel.w * 0.5f - 7, layout_.hotbarPanel.y - 7, 14);
    for (std::size_t i = 0; i < layout_.hotbarSlots.size(); ++i) {
        drawSlot(layout_.hotbarSlots[i], i == inventory_.selectedHotbar());
        drawStack(layout_.hotbarSlots[i], inventory_.slot(i));
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
