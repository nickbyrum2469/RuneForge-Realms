#ifdef _WIN32

#include "platform/windows/NativeWindow.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <windowsx.h>

namespace rf::platform {
namespace {
constexpr wchar_t kWindowClass[] = L"RuneForgeRealmsNativeWindow";

std::wstring wide(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::wstring hubTitle() {
    return L"RuneForge Realms - Vulkan Foundation " + wide(RF_VERSION_STRING);
}

std::filesystem::path executableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
}

std::optional<std::string> readVersionFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string value;
    std::getline(input, value);
    if (value.empty()) return std::nullopt;
    return value;
}

void showUpdateNoticeIfNeeded(HWND hwnd) {
    const auto runtime = executableDirectory();
    const auto previousVersion = readVersionFile(runtime.parent_path() / L"runtime.previous" / L"version.txt");
    if (!previousVersion || *previousVersion == RF_VERSION_STRING) return;

    const auto marker = runtime / (L".update-notice-" + wide(RF_VERSION_STRING) + L".ack");
    if (std::filesystem::exists(marker)) return;

    const std::wstring message = L"RuneForge Realms updated successfully from " + wide(*previousVersion) +
                                 L" to " + wide(RF_VERSION_STRING) + L".\n\n"
                                 L"The previous runtime was kept as runtime.previous for rollback.";
    MessageBoxW(hwnd, message.c_str(), L"RuneForge Realms Updated", MB_OK | MB_ICONINFORMATION);

    std::ofstream output(marker);
    output << RF_VERSION_STRING << '\n';
}
} // namespace

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
    const std::wstring title = hubTitle();
    hwnd_ = CreateWindowExW(0, kWindowClass, title.c_str(),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            desired.right - desired.left, desired.bottom - desired.top,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;

    painter_ = std::make_unique<rf::ui::HubPainter>(hwnd_);
    if (!painter_->initialize()) return false;

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    showUpdateNoticeIfNeeded(hwnd_);
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
    std::wstring title = L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Vulkan Voxel Lab - " + gpu +
                         L" - Esc: Hub | Arrows: Orbit | W/S: Zoom | Space: Auto Orbit";
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
    const std::wstring title = hubTitle();
    SetWindowTextW(hwnd_, title.c_str());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::handleAction(rf::ui::HubAction action) {
    const wchar_t* message = nullptr;
    switch (action) {
        case rf::ui::HubAction::Play:
            enterVulkanScene();
            return;
        case rf::ui::HubAction::OpenModes:
            message = L"Modes browser foundation selected. The Vulkan voxel lab is currently wired to the PLAY path.";
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
    if (message) {
        const std::wstring caption = L"RuneForge Realms " + wide(RF_VERSION_STRING);
        MessageBoxW(hwnd_, message, caption.c_str(), MB_OK | MB_ICONINFORMATION);
    }
}

} // namespace rf::platform

#endif
