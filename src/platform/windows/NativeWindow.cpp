#ifdef _WIN32

#include "platform/windows/NativeWindow.h"

#include "core/Version.h"

#include <shellapi.h>
#include <string>
#include <windowsx.h>

namespace rf::platform {
namespace {
constexpr wchar_t kWindowClass[] = L"RuneForgeRealmsNativeWindow";
}

NativeWindow::NativeWindow() = default;
NativeWindow::~NativeWindow() = default;

bool NativeWindow::create(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &NativeWindow::WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    RECT desired{0, 0, 1600, 900};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    hwnd_ = CreateWindowExW(0, kWindowClass, L"RuneForge Realms — Native Foundation 0.1.0",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            desired.right - desired.left, desired.bottom - desired.top,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;

    painter_ = std::make_unique<rf::ui::HubPainter>(hwnd_);
    if (!painter_->initialize()) return false;

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return true;
}

int NativeWindow::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK NativeWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    NativeWindow* self = reinterpret_cast<NativeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<NativeWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT NativeWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (painter_) painter_->resize(LOWORD(lParam), HIWORD(lParam));
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd_, &ps);
            if (painter_) painter_->draw();
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            if (painter_) handleAction(painter_->click(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))));
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void NativeWindow::handleAction(rf::ui::HubAction action) {
    const wchar_t* message = nullptr;
    switch (action) {
        case rf::ui::HubAction::Play:
            message = L"The native RuneForge shell is live. The next vertical-slice pass replaces this placeholder with the Vulkan voxel world, movement, mining and persistent realm loading.";
            break;
        case rf::ui::HubAction::OpenModes: message = L"Modes browser foundation selected."; break;
        case rf::ui::HubAction::OpenParty: message = L"Party / co-op shell selected. Networking is intentionally not enabled in 0.1.0."; break;
        case rf::ui::HubAction::OpenLocker: message = L"Locker / character loadout shell selected."; break;
        case rf::ui::HubAction::OpenShop: message = L"Shop is visual scaffolding only. RuneForge has no paid economy in this foundation build."; break;
        case rf::ui::HubAction::OpenSettings: message = L"Native settings panel foundation selected."; break;
        default: break;
    }
    if (message) MessageBoxW(hwnd_, message, L"RuneForge Realms 0.1.0", MB_OK | MB_ICONINFORMATION);
}

} // namespace rf::platform

#endif
