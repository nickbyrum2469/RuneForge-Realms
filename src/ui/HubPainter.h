#pragma once

#ifdef _WIN32

#include "core/HubModel.h"
#include "ui/HubLayout.h"

#include <d2d1.h>
#include <dwrite.h>
#include <string_view>
#include <windows.h>
#include <wrl/client.h>

namespace rf::ui {

enum class HubAction { None, Play, OpenModes, OpenParty, OpenLocker, OpenShop, OpenSettings };

class HubPainter {
public:
    explicit HubPainter(HWND hwnd);
    ~HubPainter();

    bool initialize();
    void resize(unsigned width, unsigned height);
    void draw();
    HubAction click(float pixelX, float pixelY);

private:
    using Microsoft::WRL::ComPtr;

    void createDeviceResources();
    void discardDeviceResources();
    void createTextResources();

    void drawBackdrop();
    void drawScenery();
    void drawLeftRail();
    void drawBrand();
    void drawProfileStrip();
    void drawFeatured();
    void drawPartyPanel();
    void drawModeCards();
    void drawNewsBar();

    void fillRect(const Rect& rect, D2D1_COLOR_F color, float radius = 0.0f);
    void strokeRect(const Rect& rect, D2D1_COLOR_F color, float thickness = 1.0f, float radius = 0.0f);
    void text(std::wstring_view value, const Rect& rect, IDWriteTextFormat* format, D2D1_COLOR_F color,
              DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING);
    void blockCluster(float x, float y, float w, float h, D2D1_COLOR_F base, int seed);

    [[nodiscard]] float scaleX() const noexcept;
    [[nodiscard]] float scaleY() const noexcept;

    HWND hwnd_{};
    unsigned pixelWidth_{1600};
    unsigned pixelHeight_{900};
    HubLayout layout_{};
    rf::HubModel model_{};

    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<IDWriteTextFormat> titleFormat_;
    ComPtr<IDWriteTextFormat> logoFormat_;
    ComPtr<IDWriteTextFormat> headingFormat_;
    ComPtr<IDWriteTextFormat> bodyFormat_;
    ComPtr<IDWriteTextFormat> smallFormat_;
};

} // namespace rf::ui

#endif
