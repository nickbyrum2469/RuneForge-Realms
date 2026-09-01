#pragma once

#ifdef _WIN32

#include <d2d1.h>
#include <dwrite.h>
#include <string_view>
#include <utility>
#include <windows.h>
#include <wrl/client.h>

namespace rf::ui::native {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct UiRect {
    float x{};
    float y{};
    float w{};
    float h{};
    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class NativeUiSurface {
public:
    explicit NativeUiSurface(HWND hwnd) : hwnd_(hwnd) {}

    bool initialize();
    void resize(unsigned width, unsigned height);
    bool begin();
    void end();

    void fill(UiRect rect, D2D1_COLOR_F color, float radius = 0.0f);
    void stroke(UiRect rect, D2D1_COLOR_F color, float thickness = 1.0f, float radius = 0.0f);
    void text(std::wstring_view value, UiRect rect, IDWriteTextFormat* format, D2D1_COLOR_F color,
              DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_CENTER);

    [[nodiscard]] std::pair<float, float> logicalPoint(int pixelX, int pixelY) const noexcept;
    [[nodiscard]] IDWriteTextFormat* titleFormat() const noexcept { return titleFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* headingFormat() const noexcept { return headingFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* bodyFormat() const noexcept { return bodyFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* smallFormat() const noexcept { return smallFormat_.Get(); }

private:
    void createTarget();
    void createTextFormats();

    HWND hwnd_{};
    unsigned width_{1600};
    unsigned height_{900};
    ComPtr<ID2D1Factory> factory_;
    ComPtr<IDWriteFactory> writeFactory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<IDWriteTextFormat> titleFormat_;
    ComPtr<IDWriteTextFormat> headingFormat_;
    ComPtr<IDWriteTextFormat> bodyFormat_;
    ComPtr<IDWriteTextFormat> smallFormat_;
};

} // namespace rf::ui::native

#endif
