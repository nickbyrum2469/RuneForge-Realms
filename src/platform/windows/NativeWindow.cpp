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
constexpr wchar_t overlayWindowClass[] = L"RuneForgeRealmsNativeUiOverlay";

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

    WNDCLASSEXW overlayClass{};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.style = CS_HREDRAW | CS_VREDRAW;
    overlayClass.lpfnWndProc = &NativeWindow::OverlayWindowProc;
    overlayClass.hInstance = instance_;
    overlayClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    overlayClass.hbrBackground = nullptr;
    overlayClass.lpszClassName = overlayWindowClass;
    if (!RegisterClassExW(&overlayClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    RECT desired{0, 0, 1600, 900};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const std::wstring title = hubTitle();
    hwnd_ = CreateWindowExW(0, windowClass, title.c_str(), WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, desired.right - desired.left, desired.bottom - desired.top,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;
    if (!createUiOverlay()) return false;

    painter_ = std::make_unique<rf::ui::HubPainter>(hwnd_);
    if (!painter_->initialize()) return false;
    painter_->setHasSave(save::frontierSaveExists(frontierSavePath()));

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    showUpdateNoticeIfNeeded(hwnd_);
    return true;
}

bool NativeWindow::createUiOverlay() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = std::max<LONG>(client.right - client.left, 1);
    const int height = std::max<LONG>(client.bottom - client.top, 1);
    uiOverlayHwnd_ = CreateWindowExW(
        WS_EX_NOPARENTNOTIFY,
        overlayWindowClass,
        L"RuneForge Native UI Overlay",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, width, height,
        hwnd_, nullptr, instance_, this);
    if (!uiOverlayHwnd_) return false;
    ShowWindow(uiOverlayHwnd_, SW_HIDE);
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
        if (uiState_.screen() == app::UiScreen::Gameplay && renderer_ && renderer_->initialized()) {
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

LRESULT CALLBACK NativeWindow::OverlayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    NativeWindow* self = reinterpret_cast<NativeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<NativeWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->uiOverlayHwnd_ = hwnd;
    }
    return self ? self->handleOverlayMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT NativeWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE: {
            const unsigned width = std::max<unsigned>(LOWORD(lParam), 1u);
            const unsigned height = std::max<unsigned>(HIWORD(lParam), 1u);
            if (painter_) painter_->resize(width, height);
            resizeUiOverlay(width, height);
            if (renderer_) renderer_->resize(width, height);
            InvalidateRect(hwnd_, nullptr, FALSE);
            if (uiState_.nativeOverlayVisible() && uiOverlayHwnd_) InvalidateRect(uiOverlayHwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd_, &ps);
            if (uiState_.screen() == app::UiScreen::Hub && painter_) painter_->draw();
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (uiState_.screen() == app::UiScreen::Hub && painter_) {
                handleAction(painter_->click(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))));
            } else if (uiState_.gameplayInputAllowed() && renderer_ && !renderer_->paused()) {
                renderer_->onMouseButtonDown(true);
            }
            return 0;

        case WM_LBUTTONUP:
            if (uiState_.gameplayInputAllowed() && renderer_) renderer_->onMouseButtonUp(true);
            return 0;

        case WM_RBUTTONDOWN:
            if (uiState_.gameplayInputAllowed() && renderer_ && !renderer_->paused()) renderer_->onMouseButtonDown(false);
            return 0;

        case WM_RBUTTONUP:
            if (uiState_.gameplayInputAllowed() && renderer_) renderer_->onMouseButtonUp(false);
            return 0;

        case WM_MOUSEMOVE:
            if (uiState_.gameplayInputAllowed() && renderer_ && mouseCaptured_ && !renderer_->paused()) {
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
            if (uiState_.nativeOverlayVisible()) return handleOverlayMessage(message, wParam, lParam);
            if (uiState_.screen() == app::UiScreen::Gameplay && renderer_) {
                if (wParam == 'I' || wParam == VK_TAB) openInventory();
                else if (wParam == VK_ESCAPE) openPause();
                else renderer_->onKeyDown(wParam);
            }
            return 0;

        case WM_KEYUP:
            if (uiState_.gameplayInputAllowed() && renderer_) renderer_->onKeyUp(wParam);
            return 0;

        case WM_ACTIVATEAPP:
            if (uiState_.screen() == app::UiScreen::Gameplay && renderer_ && wParam == FALSE) openPause();
            return 0;

        case WM_SETCURSOR:
            if (uiState_.gameplayInputAllowed() && mouseCaptured_ && renderer_ && !renderer_->paused()) {
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
            uiOverlayHwnd_ = nullptr;
            audioSystem_.shutdown();
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT NativeWindow::handleOverlayMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(uiOverlayHwnd_, &ps);
            drawActiveOverlay();
            EndPaint(uiOverlayHwnd_, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (uiState_.screen() == app::UiScreen::Pause && pausePainter_) {
                handlePauseAction(pausePainter_->hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            } else if (uiState_.screen() == app::UiScreen::Settings && settingsPainter_) {
                handleSettingsAction(settingsPainter_->hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            }
            return 0;

        case WM_KEYDOWN:
            if ((lParam & (1u << 30)) != 0 && (wParam == VK_ESCAPE || wParam == VK_TAB || wParam == 'I')) return 0;
            if (uiState_.screen() == app::UiScreen::Inventory) {
                if (wParam == 'I' || wParam == VK_TAB || wParam == VK_ESCAPE) closeInventory();
            } else if (uiState_.screen() == app::UiScreen::Pause) {
                if (wParam == VK_ESCAPE) closePause();
            } else if (uiState_.screen() == app::UiScreen::Settings) {
                if (wParam == VK_ESCAPE) closeSettings();
            }
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;

        default:
            break;
    }
    return DefWindowProcW(uiOverlayHwnd_, message, wParam, lParam);
}

void NativeWindow::drawActiveOverlay() {
    if (uiState_.screen() == app::UiScreen::Inventory && inventoryPainter_) inventoryPainter_->draw();
    else if (uiState_.screen() == app::UiScreen::Pause && pausePainter_) pausePainter_->draw();
    else if (uiState_.screen() == app::UiScreen::Settings && settingsPainter_) settingsPainter_->draw();
}

void NativeWindow::resizeUiOverlay(unsigned width, unsigned height) {
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (uiOverlayHwnd_) MoveWindow(uiOverlayHwnd_, 0, 0, static_cast<int>(width), static_cast<int>(height), TRUE);
    if (inventoryPainter_) inventoryPainter_->resize(width, height);
    if (pausePainter_) pausePainter_->resize(width, height);
    if (settingsPainter_) settingsPainter_->resize(width, height);
}

void NativeWindow::showUiOverlay() {
    if (!uiOverlayHwnd_) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    resizeUiOverlay(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                    static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    ShowWindow(uiOverlayHwnd_, SW_SHOW);
    SetWindowPos(uiOverlayHwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetFocus(uiOverlayHwnd_);
    InvalidateRect(uiOverlayHwnd_, nullptr, FALSE);
}

void NativeWindow::hideUiOverlay() {
    if (uiOverlayHwnd_) ShowWindow(uiOverlayHwnd_, SW_HIDE);
    if (hwnd_) SetFocus(hwnd_);
}

void NativeWindow::syncInteractionState() {
    if (renderer_) renderer_->setPaused(uiState_.rendererShouldBePaused());

    if (uiState_.nativeOverlayVisible()) showUiOverlay();
    else hideUiOverlay();

    if (renderer_ && uiState_.mouseShouldBeCaptured()) captureMouse();
    else releaseMouse();
}

void NativeWindow::centerMouse() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    POINT point{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
    ClientToScreen(hwnd_, &point);
    SetCursorPos(point.x, point.y);
}

void NativeWindow::captureMouse() {
    if (!hwnd_ || mouseCaptured_) return;
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
    if (uiState_.nativeOverlayVisible() && uiOverlayHwnd_) InvalidateRect(uiOverlayHwnd_, nullptr, FALSE);
    else InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::enterFrontier(bool continueExisting) {
    if (uiState_.screen() == app::UiScreen::Gameplay) return;
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
    if (!uiState_.enterGameplay()) {
        renderer_.reset();
        returnToHub();
        return;
    }
    SetWindowTextW(hwnd_, hubTitle().c_str());
    syncInteractionState();
}

void NativeWindow::openInventory() {
    if (uiState_.screen() != app::UiScreen::Gameplay || !renderer_ || !uiOverlayHwnd_) return;

    auto painter = std::make_unique<rf::ui::inventory::InventoryPainter>(uiOverlayHwnd_);
    if (!painter->initialize()) {
        MessageBoxW(hwnd_, L"RuneForge could not open the native inventory screen.", L"RuneForge Inventory", MB_OK | MB_ICONERROR);
        return;
    }
    painter->setInventory(renderer_->inventory(), renderer_->miningMode());
    RECT client{};
    GetClientRect(hwnd_, &client);
    painter->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                    static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));

    inventoryPainter_ = std::move(painter);
    if (!uiState_.openInventory()) {
        inventoryPainter_.reset();
        return;
    }
    SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Inventory").c_str());
    syncInteractionState();
}

void NativeWindow::closeInventory() {
    if (uiState_.screen() != app::UiScreen::Inventory || !renderer_) return;
    inventoryPainter_.reset();
    if (!uiState_.closeInventory()) return;
    SetWindowTextW(hwnd_, hubTitle().c_str());
    syncInteractionState();
}

bool NativeWindow::createPausePainter() {
    if (!uiOverlayHwnd_) return false;
    auto painter = std::make_unique<rf::ui::menus::PauseMenuPainter>(uiOverlayHwnd_);
    if (!painter->initialize()) return false;
    RECT client{};
    GetClientRect(hwnd_, &client);
    painter->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                    static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
    pausePainter_ = std::move(painter);
    return true;
}

void NativeWindow::openPause() {
    if (uiState_.screen() != app::UiScreen::Gameplay || !renderer_) return;
    if (!createPausePainter()) {
        MessageBoxW(hwnd_, L"RuneForge could not open the pause menu.", L"RuneForge Pause", MB_OK | MB_ICONERROR);
        return;
    }
    if (!uiState_.openPause()) {
        pausePainter_.reset();
        return;
    }
    SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Paused").c_str());
    syncInteractionState();
}

void NativeWindow::closePause() {
    if (uiState_.screen() != app::UiScreen::Pause || !renderer_) return;
    pausePainter_.reset();
    if (!uiState_.closePause()) return;
    SetWindowTextW(hwnd_, hubTitle().c_str());
    syncInteractionState();
}

void NativeWindow::openSettings() {
    const app::UiScreen returnScreen = uiState_.screen();
    if (returnScreen != app::UiScreen::Hub && returnScreen != app::UiScreen::Pause) return;
    if (!uiOverlayHwnd_) return;

    if (returnScreen == app::UiScreen::Pause) pausePainter_.reset();
    auto painter = std::make_unique<rf::ui::settings::SettingsPainter>(uiOverlayHwnd_);
    if (!painter->initialize()) {
        if (returnScreen == app::UiScreen::Pause && !createPausePainter()) {
            (void)uiState_.closePause();
            syncInteractionState();
        }
        MessageBoxW(hwnd_, L"RuneForge could not open the settings screen.", L"RuneForge Settings", MB_OK | MB_ICONERROR);
        return;
    }
    painter->setSettings(settings_);
    RECT client{};
    GetClientRect(hwnd_, &client);
    painter->resize(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                    static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));

    settingsPainter_ = std::move(painter);
    if (!uiState_.openSettings()) {
        settingsPainter_.reset();
        if (returnScreen == app::UiScreen::Pause) (void)createPausePainter();
        return;
    }
    SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Settings").c_str());
    syncInteractionState();
}

void NativeWindow::closeSettings() {
    if (uiState_.screen() != app::UiScreen::Settings) return;
    const app::UiScreen returnScreen = uiState_.settingsReturnScreen();
    settingsPainter_.reset();
    if (!uiState_.closeSettings()) return;

    if (returnScreen == app::UiScreen::Pause) {
        if (!createPausePainter()) {
            (void)uiState_.closePause();
            SetWindowTextW(hwnd_, hubTitle().c_str());
            syncInteractionState();
            MessageBoxW(hwnd_, L"The pause menu could not be restored, so RuneForge returned safely to gameplay.",
                        L"RuneForge Pause", MB_OK | MB_ICONWARNING);
            return;
        }
        SetWindowTextW(hwnd_, (L"RuneForge Realms " + wide(RF_VERSION_STRING) + L" - Paused").c_str());
    } else {
        SetWindowTextW(hwnd_, hubTitle().c_str());
    }
    syncInteractionState();
    if (uiState_.screen() == app::UiScreen::Hub) InvalidateRect(hwnd_, nullptr, FALSE);
}

void NativeWindow::handlePauseAction(rf::ui::menus::PauseAction action) {
    switch (action) {
        case rf::ui::menus::PauseAction::Resume: closePause(); return;
        case rf::ui::menus::PauseAction::Settings: openSettings(); return;
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
    uiState_.returnToHub();
    syncInteractionState();
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
    SetWindowTextW(hwnd_, hubTitle().c_str());
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
        case rf::ui::HubAction::OpenSettings: openSettings(); return;
        case rf::ui::HubAction::Quit: PostMessageW(hwnd_, WM_CLOSE, 0, 0); return;
        default: return;
    }
}

} // namespace rf::platform

#endif
