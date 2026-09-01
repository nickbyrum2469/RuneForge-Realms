#ifdef _WIN32

#include "platform/windows/NativeWindow.h"

#include <string>
#include <windowsx.h>

namespace rf::platform {
namespace {
constexpr wchar_t kWindowClass[] = L"RuneForgeRealmsNativeWindow";
constexpr wchar_t kHubTitle[] = L"RuneForge Realms — Vulkan Foundation 0.2.0";
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
    hwnd_ = CreateWindowExW(0, kWindowClass, kHubTitle,
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
    for (;;) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return static_cast<int>(message.wParam);
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (mode_ == ViewMode::VulkanScene && renderer_ && renderer_->initialized()) {
            renderer_->drawFrame();
        } else {
            WaitMessage();
        }
    }
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

        case WM_SIZE: {
            const unsigned width = LOWORD(lParam);
            const unsigned height = HIWORD(lParam);
            if (painter_) painter_->resize(width, height);
            if (renderer_) renderer_->resize(width, height);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd_, &ps);
            if (mode_ == ViewMode::Hub && painter_) painter_->draw();
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (mode_ == ViewMode::Hub && painter_) {
                handleAction(painter_->click(static_cast<float>(GET_X_LPARAM(lParam)),
                                             static_cast<float>(GET_Y_LPARAM(lParam))));
            }
            return 0;

        case WM_KEYDOWN:
            if (mode_ == ViewMode::VulkanScene) {
                if (wParam == VK_ESCAPE) {
                    returnToHub();
                } else if (renderer_) {
                    renderer_->onKeyDown(wParam);
                }
            } else if (wParam == VK_ESCAPE) {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            }
            return 0;

        case WM_DESTROY:
            renderer_.reset();
            painter_.reset();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void NativeWindow::enterVulkanScene() {
    if (mode_ == ViewMode::VulkanScene) return;

    // Direct2D and Vulkan deliberately do not own the HWND at the same time.
    painter_.reset();
    renderer_ = std::make_unique<rf::render::VulkanRenderer>(hwnd_);
    if (!renderer_->initialize()) {
        const std::wstring error = renderer_->lastError().empty()
            ? L"The Vulkan renderer could not initialize."
            : renderer_->lastError();
        renderer_.reset();
        returnToHub();
        MessageBoxW(hwnd_, error.c_str(), L"RuneForge Vulkan initialization", MB_OK | MB_ICONERROR);
        return;
    }

    mode_ = ViewMode::VulkanScene;
    std::wstring gpu(renderer_->gpuName().begin(), renderer_->gpuName().end());
    std::wstring title = L"RuneForge Realms 0.2.0 — Vulkan Voxel Lab — " + gpu +
                         L" — Esc: Hub | Arrows: Orbit | W/S: Zoom | Space: Auto Orbit";
    SetWindowTextW(hwnd_, title.c_str());
}

void NativeWindow::returnToHub() {
    renderer_.reset();
    mode_ = ViewMode::Hub;

    painter_ = std::make_unique<rf::ui::HubPainter>(hwnd_);
    if (!painter_->initialize()) {
        MessageBoxW(hwnd_, L"RuneForge could not restore the native hub renderer.",
                    L"RuneForge Realms", MB_OK | MB_ICONERROR);
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    painter_->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                     static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    SetWindowTextW(hwnd_, kHubTitle);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::handleAction(rf::ui::HubAction action) {
    const wchar_t* message = nullptr;
    switch (action) {
        case rf::ui::HubAction::Play:
            enterVulkanScene();
            return;
        case rf::ui::HubAction::OpenModes:
            message = L"Modes browser foundation selected. The first Vulkan voxel lab is now wired to the PLAY path.";
            break;
        case rf::ui::HubAction::OpenParty:
            message = L"Party / co-op shell selected. Networking stays isolated until the local world simulation is stable.";
            break;
        case rf::ui::HubAction::OpenLocker:
            message = L"Locker / character loadout shell selected.";
            break;
        case rf::ui::HubAction::OpenShop:
            message = L"Shop is visual scaffolding only. RuneForge has no paid economy in this foundation build.";
            break;
        case rf::ui::HubAction::OpenSettings:
            message = L"Native settings panel foundation selected. Vulkan quality/device controls come after renderer diagnostics.";
            break;
        default:
            break;
    }
    if (message) MessageBoxW(hwnd_, message, L"RuneForge Realms 0.2.0", MB_OK | MB_ICONINFORMATION);
}

} // namespace rf::platform

#endif
