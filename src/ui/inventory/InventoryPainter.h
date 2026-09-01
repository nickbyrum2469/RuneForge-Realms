#pragma once

#ifdef _WIN32

#include "ui/inventory/InventoryLayout.h"

#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <string_view>
#include <windows.h>
#include <wrl/client.h>

namespace rf::ui::inventory {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class InventoryPainter {
public:
    explicit InventoryPainter(HWND hwnd);
    ~InventoryPainter();

    bool initialize();
    void resize(unsigned width, unsigned height);
    void draw();

private:
    void createTextResources();
    void createDeviceResources();
    void discardDeviceResources();

    void drawBackdrop();
    void drawOuterFrame();
    void drawEquipment();
    void drawInventoryGrid();
    void drawQuickCraft();
    void drawHotbar();
    void drawSlot(const Rect& rect, bool selected = false);
    void drawItemCube(const Rect& rect, unsigned top, unsigned side, unsigned edge);
    void drawGem(float x, float y, float size);
    void drawDivider(float x, float y, float width);

    void fillRect(const Rect& rect, D2D1_COLOR_F color, float radius = 0.0f);
    void strokeRect(const Rect& rect, D2D1_COLOR_F color, float thickness = 1.0f, float radius = 0.0f);
    void fillGradient(const Rect& rect, D2D1_COLOR_F top, D2D1_COLOR_F bottom);
    void text(std::wstring_view value, const Rect& rect, IDWriteTextFormat* format, D2D1_COLOR_F color,
              DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING);

    HWND hwnd_{};
    unsigned pixelWidth_{1600};
    unsigned pixelHeight_{900};
    InventoryLayout layout_{};

    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<IDWriteTextFormat> titleFormat_;
    ComPtr<IDWriteTextFormat> headingFormat_;
    ComPtr<IDWriteTextFormat> bodyFormat_;
    ComPtr<IDWriteTextFormat> smallFormat_;
    ComPtr<IDWriteTextFormat> tinyFormat_;
};

} // namespace rf::ui::inventory

#endif
