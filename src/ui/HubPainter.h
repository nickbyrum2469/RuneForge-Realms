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

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

enum class HubAction { None, ContinueGame, NewGame, OpenWorlds, OpenSettings, Quit };

class HubPainter {
public:
    explicit HubPainter(HWND hwnd);
    ~HubPainter();

    bool initialize();
    void resize(unsigned width, unsigned height);
    void draw();
    HubAction click(float pixelX, float pixelY);
    void setHasSave(bool value) noexcept { model_.setHasSave(value); }

private:
    void createDeviceResources();
    void discardDeviceResources();
    void createTextResources();

    void drawBackdrop();
    void drawScenery();
    void drawLeftRail();
    void drawBrand();
    void drawProfileStrip();
    void drawFeatured();
    void drawStatusPanel();
    void drawFeatureCards();
    void drawNewsBar();
    void drawCastle(float x, float y, float scale, D2D1_COLOR_F stone);

    void fillRect(const Rect& rect, D2D1_COLOR_F color, float radius = 0.0f);
    void strokeRect(const Rect& rect, D2D1_COLOR_F color, float thickness = 1.0f, float radius = 0.0f);
    void fillGradient(const Rect& rect, D2D1_COLOR_F top, D2D1_COLOR_F bottom);
    void text(std::wstring_view value, const Rect& rect, IDWriteTextFormat* format, D2D1_COLOR_F color,
              DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING);
    void blockCluster(float x, float y, float w, float h, D2D1_COLOR_F base, int seed, float cell = 9.0f);

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
    ComPtr<IDWriteTextFormat> tinyFormat_;
};

} // namespace rf::ui

#endif
