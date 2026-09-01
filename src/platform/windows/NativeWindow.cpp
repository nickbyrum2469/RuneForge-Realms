#ifdef _WIN32

#include "platform/windows/NativeWindow.h"

#include "save/FrontierSave.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <windowsx.h>

namespace rf::platform {
namespace {
constexpr wchar_t windowClass[] = L"RuneForgeRealmsNativeWindow";

std::wstring wide(std::string_view value) { return std::wstring(value.begin(), value.end()); }
std::wstring hubTitle() { return L"RuneForge Realms - Frontier Realms " + wide(RF_VERSION_STRING); }

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
    return value.empty() ? std::nullopt : std::optional<std::string>{value};
}

void showUpdateNoticeIfNeeded(HWND hwnd) {
    const auto runtime = executableDirectory();
    const auto previous = readVersionFile(runtime.parent_path() / L"runtime.previous" / L"version.txt");
    if (!previous || *previous == RF_VERSION_STRING) return;
    const auto marker = runtime / (L".update-notice-" + wide(RF_VERSION_STRING) + L".ack");
    if (std::filesystem::exists(marker)) return;

    const std::wstring message = L"RuneForge Realms updated successfully from " + wide(*previous) + L" to " +
                                 wide(RF_VERSION_STRING) + L".\n\nThe previous runtime was kept for rollback.";
    MessageBoxW(hwnd, message.c_str(), L"RuneForge Realms Updated", MB_OK | MB_ICONINFORMATION);
    std::ofstream output(marker);
    output << RF_VERSION_STRING << '\n';
}
} // namespace

NativeWindow::NativeWindow() = default;
NativeWindow::~NativeWindow() = default;

std::filesystem::path NativeWindow::frontierSavePath() const {
    if (const wchar_t* local = _wgetenv(L"LOCALAPPDATA")) {
        return std::filesystem::path(local) / L"RuneForgeRealms" / L"Saves" / L"FrontierRealms" / L"world.rfsv";
    }
    return executableDirectory() / L"user-data" / L"FrontierRealms" / L"world.rfsv";
}

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
    wc.lpszClassName = windowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    RECT desired{0, 0, 1600, 900};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const std::wstring title = hubTitle();
    hwnd_ = CreateWindowExW(0, windowClass, title.c_str(), WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, desired.right - desired.left, desired.bottom - desired.top,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;

    painter_ = std::make_unique<rf::ui::HubPainter>(hwnd_);
    if (!painter_->initialize()) return false;
    painter_->setHasSave(save::frontierSaveExists(frontierSavePath()));

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
        if (mode_ == ViewMode::Frontier && renderer_ && renderer_->initialized()) renderer_->drawFrame();
        else WaitMessage();
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
            if (inventoryPainter_) inventoryPainter_->resize(width, height);
            if (renderer_) renderer_->resize(width, height);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd_, &ps);
            if (mode_ == ViewMode::Hub && painter_) painter_->draw();
            else if (mode_ == ViewMode::Inventory && inventoryPainter_) inventoryPainter_->draw();
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (mode_ == ViewMode::Hub && painter_) {
                handleAction(painter_->click(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))));
            } else if (mode_ == ViewMode::Frontier && renderer_ && !renderer_->paused()) {
                renderer_->onMouseButton(true);
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (mode_ == ViewMode::Frontier && renderer_ && !renderer_->paused()) renderer_->onMouseButton(false);
            return 0;

        case WM_MOUSEMOVE:
            if (mode_ == ViewMode::Frontier && renderer_ && mouseCaptured_ && !renderer_->paused()) {
                RECT client{};
                GetClientRect(hwnd_, &client);
                const int centerX = (client.right - client.left) / 2;
                const int centerY = (client.bottom - client.top) / 2;
                const int dx = GET_X_LPARAM(lParam) - centerX;
                const int dy = GET_Y_LPARAM(lParam) - centerY;
                if (dx != 0 || dy != 0) {
                    renderer_->onMouseDelta(static_cast<float>(dx), static_cast<float>(dy));
                    centerMouse();
                }
            }
            return 0;

        case WM_KEYDOWN:
            if (mode_ == ViewMode::Inventory) {
                if ((wParam == 'I' || wParam == VK_ESCAPE) && (lParam & (1u << 30)) == 0) closeInventory();
                return 0;
            }

            if (mode_ == ViewMode::Frontier && renderer_) {
                if (wParam == 'I' && (lParam & (1u << 30)) == 0 && !renderer_->paused()) {
                    openInventory();
                } else if (wParam == VK_ESCAPE && (lParam & (1u << 30)) == 0) {
                    const bool pause = !renderer_->paused();
                    renderer_->setPaused(pause);
                    if (pause) releaseMouse(); else captureMouse();
                } else if (renderer_->paused() && wParam == 'H') {
                    returnToHub();
                } else {
                    renderer_->onKeyDown(wParam);
                }
            }
            return 0;

        case WM_KEYUP:
            if (mode_ == ViewMode::Frontier && renderer_) renderer_->onKeyUp(wParam);
            return 0;

        case WM_ACTIVATEAPP:
            if (mode_ == ViewMode::Frontier && renderer_ && wParam == FALSE) {
                renderer_->setPaused(true);
                releaseMouse();
            }
            return 0;

        case WM_SETCURSOR:
            if (mode_ == ViewMode::Frontier && mouseCaptured_ && renderer_ && !renderer_->paused()) {
                SetCursor(LoadCursor(nullptr, IDC_CROSS));
                return TRUE;
            }
            if (mode_ == ViewMode::Inventory) {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                return TRUE;
            }
            break;

        case WM_DESTROY:
            releaseMouse();
            inventoryPainter_.reset();
            renderer_.reset();
            painter_.reset();
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void NativeWindow::centerMouse() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    POINT point{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
    ClientToScreen(hwnd_, &point);
    SetCursorPos(point.x, point.y);
}

void NativeWindow::captureMouse() {
    if (!hwnd_) return;
    SetCapture(hwnd_);
    RECT rect{};
    GetClientRect(hwnd_, &rect);
    POINT topLeft{rect.left, rect.top};
    POINT bottomRight{rect.right, rect.bottom};
    ClientToScreen(hwnd_, &topLeft);
    ClientToScreen(hwnd_, &bottomRight);
    RECT clip{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&clip);
    mouseCaptured_ = true;
    SetCursor(LoadCursor(nullptr, IDC_CROSS));
    centerMouse();
}

void NativeWindow::releaseMouse() {
    if (mouseCaptured_) ReleaseCapture();
    ClipCursor(nullptr);
    mouseCaptured_ = false;
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
}

void NativeWindow::enterFrontier(bool continueExisting) {
    if (mode_ == ViewMode::Frontier) return;
    painter_.reset();
    inventoryPainter_.reset();
    renderer_ = std::make_unique<rf::render::VulkanRenderer>(hwnd_, frontierSavePath(), continueExisting);
    if (!renderer_->initialize()) {
        const std::wstring error = renderer_->lastError().empty() ? L"Frontier Realms could not initialize." : renderer_->lastError();
        renderer_.reset();
        returnToHub();
        MessageBoxW(hwnd_, error.c_str(), L"RuneForge Frontier initialization", MB_OK | MB_ICONERROR);
        return;
    }
    mode_ = ViewMode::Frontier;
    captureMouse();
}

void NativeWindow::openInventory() {
    if (mode_ != ViewMode::Frontier || !renderer_) return;
    renderer_->setPaused(true);
    releaseMouse();

    inventoryPainter_ = std::make_unique<rf::ui::inventory::InventoryPainter>(hwnd_);
    if (!inventoryPainter_->initialize()) {
        inventoryPainter_.reset();
        renderer_->setPaused(false);
        captureMouse();
        MessageBoxW(hwnd_, L"RuneForge could not open the native inventory screen.", L"RuneForge Inventory", MB_OK | MB_ICONERROR);
        return;
    }
    inventoryPainter_->setInventory(renderer_->inventory(), renderer_->miningMode());

    RECT client{};
    GetClientRect(hwnd_, &client);
    inventoryPainter_->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                              static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    mode_ = ViewMode::Inventory;
    SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Inventory").c_str());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::closeInventory() {
    if (mode_ != ViewMode::Inventory || !renderer_) return;
    inventoryPainter_.reset();
    mode_ = ViewMode::Frontier;
    renderer_->setPaused(false);
    captureMouse();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::returnToHub() {
    inventoryPainter_.reset();
    if (renderer_) renderer_->saveNow();
    renderer_.reset();
    releaseMouse();
    mode_ = ViewMode::Hub;
    painter_ = std::make_unique<rf::ui::HubPainter>(hwnd_);
    if (!painter_->initialize()) {
        MessageBoxW(hwnd_, L"RuneForge could not restore the native main menu.", L"RuneForge Realms", MB_OK | MB_ICONERROR);
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        return;
    }
    painter_->setHasSave(save::frontierSaveExists(frontierSavePath()));
    RECT client{};
    GetClientRect(hwnd_, &client);
    painter_->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                     static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    const std::wstring title = hubTitle();
    SetWindowTextW(hwnd_, title.c_str());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::handleAction(rf::ui::HubAction action) {
    switch (action) {
        case rf::ui::HubAction::ContinueGame:
            enterFrontier(true);
            return;
        case rf::ui::HubAction::NewGame:
            if (save::frontierSaveExists(frontierSavePath())) {
                const int choice = MessageBoxW(hwnd_, L"Start a new Frontier?\n\nYour current world will remain until the new world is saved, then it will be replaced.",
                                               L"New Frontier", MB_YESNO | MB_ICONQUESTION);
                if (choice != IDYES) return;
            }
            enterFrontier(false);
            return;
        case rf::ui::HubAction::OpenWorlds:
            MessageBoxW(hwnd_, L"World management is being expanded around Frontier's persistent save. Continue and New World are active now.",
                        L"RuneForge Worlds", MB_OK | MB_ICONINFORMATION);
            return;
        case rf::ui::HubAction::OpenSettings:
            MessageBoxW(hwnd_, L"Graphics, input and accessibility settings are being built into the native UI.\n\nFrontier controls: WASD, mouse, Space, Shift, Ctrl, I inventory, M mining mode, LMB mine, RMB place, 1-9 hotbar, F5 save, Esc pause.",
                        L"RuneForge Settings", MB_OK | MB_ICONINFORMATION);
            return;
        case rf::ui::HubAction::Quit:
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return;
        default:
            return;
    }
}

} // namespace rf::platform

#endif
