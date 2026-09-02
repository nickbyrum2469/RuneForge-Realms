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

std::filesystem::path NativeWindow::settingsPath() const {
    if (const wchar_t* local = _wgetenv(L"LOCALAPPDATA")) {
        return std::filesystem::path(local) / L"RuneForgeRealms" / L"settings.cfg";
    }
    return executableDirectory() / L"user-data" / L"settings.cfg";
}

bool NativeWindow::create(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    settings_ = core::settings::loadGameSettings(settingsPath());
    // Audio failure must never prevent the visual game from booting. The semantic event queue remains
    // valid and can be consumed after a device becomes available again.
    (void)audioSystem_.initialize();

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
        if (mode_ == ViewMode::Frontier && renderer_ && renderer_->initialized()) {
            renderer_->drawFrame();
            audioSystem_.consume(renderer_->drainAudioEvents(), renderer_->audioListenerPosition());
            audioSystem_.update();
        } else {
            audioSystem_.update();
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
            if (inventoryPainter_) inventoryPainter_->resize(width, height);
            if (pausePainter_) pausePainter_->resize(width, height);
            if (settingsPainter_) settingsPainter_->resize(width, height);
            if (renderer_) renderer_->resize(width, height);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd_, &ps);
            if (mode_ == ViewMode::Hub && painter_) painter_->draw();
            else if (mode_ == ViewMode::Inventory && inventoryPainter_) inventoryPainter_->draw();
            else if (mode_ == ViewMode::Pause && pausePainter_) pausePainter_->draw();
            else if (mode_ == ViewMode::Settings && settingsPainter_) settingsPainter_->draw();
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (mode_ == ViewMode::Hub && painter_) {
                handleAction(painter_->click(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))));
            } else if (mode_ == ViewMode::Frontier && renderer_ && !renderer_->paused()) {
                renderer_->onMouseButtonDown(true);
            } else if (mode_ == ViewMode::Pause && pausePainter_) {
                handlePauseAction(pausePainter_->hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            } else if (mode_ == ViewMode::Settings && settingsPainter_) {
                handleSettingsAction(settingsPainter_->hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            }
            return 0;

        case WM_LBUTTONUP:
            if (mode_ == ViewMode::Frontier && renderer_) renderer_->onMouseButtonUp(true);
            return 0;

        case WM_RBUTTONDOWN:
            if (mode_ == ViewMode::Frontier && renderer_ && !renderer_->paused()) renderer_->onMouseButtonDown(false);
            return 0;

        case WM_RBUTTONUP:
            if (mode_ == ViewMode::Frontier && renderer_) renderer_->onMouseButtonUp(false);
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
            if ((lParam & (1u << 30)) != 0 && (wParam == VK_ESCAPE || wParam == VK_TAB || wParam == 'I')) return 0;

            if (mode_ == ViewMode::Inventory) {
                if (wParam == 'I' || wParam == VK_TAB || wParam == VK_ESCAPE) closeInventory();
                return 0;
            }
            if (mode_ == ViewMode::Pause) {
                if (wParam == VK_ESCAPE) closePause();
                return 0;
            }
            if (mode_ == ViewMode::Settings) {
                if (wParam == VK_ESCAPE) closeSettings();
                return 0;
            }
            if (mode_ == ViewMode::Frontier && renderer_) {
                if ((wParam == 'I' || wParam == VK_TAB) && !renderer_->paused()) openInventory();
                else if (wParam == VK_ESCAPE) openPause();
                else renderer_->onKeyDown(wParam);
            }
            return 0;

        case WM_KEYUP:
            if (mode_ == ViewMode::Frontier && renderer_) renderer_->onKeyUp(wParam);
            return 0;

        case WM_ACTIVATEAPP:
            if (mode_ == ViewMode::Frontier && renderer_ && wParam == FALSE) openPause();
            return 0;

        case WM_SETCURSOR:
            if (mode_ == ViewMode::Frontier && mouseCaptured_ && renderer_ && !renderer_->paused()) {
                SetCursor(nullptr);
                return TRUE;
            }
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;

        case WM_DESTROY:
            releaseMouse();
            settingsPainter_.reset();
            pausePainter_.reset();
            inventoryPainter_.reset();
            renderer_.reset();
            painter_.reset();
            audioSystem_.shutdown();
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
    SetCursor(nullptr);
    centerMouse();
}

void NativeWindow::releaseMouse() {
    if (mouseCaptured_) ReleaseCapture();
    ClipCursor(nullptr);
    mouseCaptured_ = false;
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
}

void NativeWindow::applySettings() {
    settings_.sanitize();
    (void)core::settings::saveGameSettings(settingsPath(), settings_);
    if (renderer_) renderer_->applySettings(settings_);
    if (settingsPainter_) settingsPainter_->setSettings(settings_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::enterFrontier(bool continueExisting) {
    if (mode_ == ViewMode::Frontier) return;
    painter_.reset();
    inventoryPainter_.reset();
    pausePainter_.reset();
    settingsPainter_.reset();
    if (!audioSystem_.initialized()) (void)audioSystem_.initialize();
    renderer_ = std::make_unique<rf::render::VulkanRenderer>(hwnd_, frontierSavePath(), continueExisting);
    renderer_->applySettings(settings_);
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

void NativeWindow::openPause() {
    if (mode_ != ViewMode::Frontier || !renderer_) return;
    renderer_->setPaused(true);
    releaseMouse();
    pausePainter_ = std::make_unique<rf::ui::menus::PauseMenuPainter>(hwnd_);
    if (!pausePainter_->initialize()) {
        pausePainter_.reset();
        renderer_->setPaused(false);
        captureMouse();
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    pausePainter_->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                          static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    mode_ = ViewMode::Pause;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::closePause() {
    if (mode_ != ViewMode::Pause || !renderer_) return;
    pausePainter_.reset();
    mode_ = ViewMode::Frontier;
    renderer_->setPaused(false);
    captureMouse();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::openSettings(ViewMode returnMode) {
    settingsReturnMode_ = returnMode;
    if (returnMode == ViewMode::Frontier && renderer_) renderer_->setPaused(true);
    releaseMouse();
    settingsPainter_ = std::make_unique<rf::ui::settings::SettingsPainter>(hwnd_);
    if (!settingsPainter_->initialize()) {
        settingsPainter_.reset();
        return;
    }
    settingsPainter_->setSettings(settings_);
    RECT client{};
    GetClientRect(hwnd_, &client);
    settingsPainter_->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                             static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    mode_ = ViewMode::Settings;
    SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Settings").c_str());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::closeSettings() {
    if (mode_ != ViewMode::Settings) return;
    settingsPainter_.reset();
    if (settingsReturnMode_ == ViewMode::Pause && renderer_) {
        mode_ = ViewMode::Pause;
        SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Paused").c_str());
    } else {
        mode_ = ViewMode::Hub;
        const std::wstring title = hubTitle();
        SetWindowTextW(hwnd_, title.c_str());
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::handlePauseAction(rf::ui::menus::PauseAction action) {
    switch (action) {
        case rf::ui::menus::PauseAction::Resume: closePause(); return;
        case rf::ui::menus::PauseAction::Settings: openSettings(ViewMode::Pause); return;
        case rf::ui::menus::PauseAction::ReturnToMain: returnToHub(); return;
        case rf::ui::menus::PauseAction::Quit:
            if (renderer_) renderer_->saveNow();
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return;
        case rf::ui::menus::PauseAction::None: return;
    }
}

void NativeWindow::handleSettingsAction(rf::ui::settings::SettingsAction action) {
    switch (action) {
        case rf::ui::settings::SettingsAction::SensitivityDown: settings_.mouseSensitivity -= 0.10f; break;
        case rf::ui::settings::SettingsAction::SensitivityUp: settings_.mouseSensitivity += 0.10f; break;
        case rf::ui::settings::SettingsAction::FovDown: settings_.fovDegrees -= 5.0f; break;
        case rf::ui::settings::SettingsAction::FovUp: settings_.fovDegrees += 5.0f; break;
        case rf::ui::settings::SettingsAction::FoliageCycle: settings_.foliageQuality = (settings_.foliageQuality + 1) % 3; break;
        case rf::ui::settings::SettingsAction::Back: closeSettings(); return;
        case rf::ui::settings::SettingsAction::None: return;
    }
    applySettings();
}

void NativeWindow::returnToHub() {
    settingsPainter_.reset();
    pausePainter_.reset();
    inventoryPainter_.reset();
    if (renderer_) renderer_->saveNow();
    renderer_.reset();
    audioSystem_.stopAll();
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
        case rf::ui::HubAction::ContinueGame: enterFrontier(true); return;
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
        case rf::ui::HubAction::OpenSettings: openSettings(ViewMode::Hub); return;
        case rf::ui::HubAction::Quit: PostMessageW(hwnd_, WM_CLOSE, 0, 0); return;
        default: return;
    }
}

} // namespace rf::platform

#endif
