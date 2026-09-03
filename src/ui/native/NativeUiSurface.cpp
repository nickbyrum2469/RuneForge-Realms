#ifdef _WIN32

#include "ui/native/NativeUiSurface.h"

#include <algorithm>

namespace rf::ui::native {

bool NativeUiSurface::initialize() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(writeFactory_.ReleaseAndGetAddressOf())))) return false;
    createTextFormats();
    createTarget();
    return target_ != nullptr;
}

void NativeUiSurface::createTextFormats() {
    auto make = [this](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                       ComPtr<IDWriteTextFormat>& output) {
        writeFactory_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, size, L"en-us",
                                        output.ReleaseAndGetAddressOf());
        if (output) {
            output->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            output->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    };
    // Georgia gives the native screens a carved-fantasy title shape much closer to the supplied
    // RuneForge reference while remaining a standard Windows font. Body copy stays highly legible.
    make(L"Georgia", 42.0f, DWRITE_FONT_WEIGHT_BOLD, titleFormat_);
    make(L"Georgia", 21.0f, DWRITE_FONT_WEIGHT_BOLD, headingFormat_);
    make(L"Segoe UI", 15.0f, DWRITE_FONT_WEIGHT_NORMAL, bodyFormat_);
    make(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, smallFormat_);
}

void NativeUiSurface::createTarget() {
    if (target_ || !factory_) return;
    RECT rect{};
    GetClientRect(hwnd_, &rect);
    width_ = static_cast<unsigned>(std::max(1L, rect.right - rect.left));
    height_ = static_cast<unsigned>(std::max(1L, rect.bottom - rect.top));
    factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(width_, height_)),
        target_.ReleaseAndGetAddressOf());
}

void NativeUiSurface::resize(unsigned width, unsigned height) {
    width_ = std::max(width, 1u);
    height_ = std::max(height, 1u);
    if (target_) target_->Resize(D2D1::SizeU(width_, height_));
}

bool NativeUiSurface::begin() {
    createTarget();
    if (!target_) return false;
    target_->BeginDraw();
    const float sx = static_cast<float>(width_) / 1600.0f;
    const float sy = static_cast<float>(height_) / 900.0f;
    target_->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy));
    return true;
}

void NativeUiSurface::end() {
    if (!target_) return;
    const HRESULT result = target_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) target_.Reset();
}

void NativeUiSurface::fill(UiRect rect, D2D1_COLOR_F color, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.ReleaseAndGetAddressOf());
    const auto r = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0.0f) target_->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush.Get());
    else target_->FillRectangle(r, brush.Get());
}

void NativeUiSurface::stroke(UiRect rect, D2D1_COLOR_F color, float thickness, float radius) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.ReleaseAndGetAddressOf());
    const auto r = D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    if (radius > 0.0f) target_->DrawRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush.Get(), thickness);
    else target_->DrawRectangle(r, brush.Get(), thickness);
}

void NativeUiSurface::line(float x0, float y0, float x1, float y1, D2D1_COLOR_F color, float thickness) {
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.ReleaseAndGetAddressOf());
    target_->DrawLine(D2D1::Point2F(x0, y0), D2D1::Point2F(x1, y1), brush.Get(), thickness);
}

void NativeUiSurface::diamond(float cx, float cy, float radius, D2D1_COLOR_F fillColor,
                              D2D1_COLOR_F strokeColor, float strokeThickness) {
    if (!factory_ || !target_) return;
    ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory_->CreatePathGeometry(geometry.ReleaseAndGetAddressOf())) || !geometry) return;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.ReleaseAndGetAddressOf())) || !sink) return;
    sink->BeginFigure(D2D1::Point2F(cx, cy - radius), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(cx + radius, cy));
    sink->AddLine(D2D1::Point2F(cx, cy + radius));
    sink->AddLine(D2D1::Point2F(cx - radius, cy));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
    target_->CreateSolidColorBrush(fillColor, fillBrush.ReleaseAndGetAddressOf());
    target_->CreateSolidColorBrush(strokeColor, strokeBrush.ReleaseAndGetAddressOf());
    target_->FillGeometry(geometry.Get(), fillBrush.Get());
    target_->DrawGeometry(geometry.Get(), strokeBrush.Get(), strokeThickness);
}

void NativeUiSurface::text(std::wstring_view value, UiRect rect, IDWriteTextFormat* format,
                           D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT alignment) {
    if (!format) return;
    ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(color, brush.ReleaseAndGetAddressOf());
    format->SetTextAlignment(alignment);
    target_->DrawTextW(value.data(), static_cast<UINT32>(value.size()), format,
                       D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h), brush.Get());
}

std::pair<float, float> NativeUiSurface::logicalPoint(int pixelX, int pixelY) const noexcept {
    return {static_cast<float>(pixelX) * 1600.0f / static_cast<float>(std::max(width_, 1u)),
            static_cast<float>(pixelY) * 900.0f / static_cast<float>(std::max(height_, 1u))};
}

} // namespace rf::ui::native

#endif
