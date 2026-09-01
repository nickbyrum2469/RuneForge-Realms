#pragma once

#ifdef _WIN32

#include "core/settings/GameSettings.h"
#include "render/vulkan/VulkanRenderer.h"
#include "ui/HubPainter.h"
#include "ui/inventory/InventoryPainter.h"
#include "ui/menus/PauseMenuPainter.h"
#include "ui/settings/SettingsPainter.h"

#include <filesystem>
#include <memory>
#include <windows.h>

namespace rf::platform {

class NativeWindow {
public:
    NativeWindow();
    ~NativeWindow();

    bool create(HINSTANCE instance, int showCommand);
    int run();

private:
    enum class ViewMode { Hub, Frontier, Inventory, Pause, Settings };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleAction(rf::ui::HubAction action);
    void handlePauseAction(rf::ui::menus::PauseAction action);
    void handleSettingsAction(rf::ui::settings::SettingsAction action);
    void enterFrontier(bool continueExisting);
    void openInventory();
    void closeInventory();
    void openPause();
    void closePause();
    void openSettings(ViewMode returnMode);
    void closeSettings();
    void applySettings();
    void returnToHub();
    void captureMouse();
    void releaseMouse();
    void centerMouse();
    [[nodiscard]] std::filesystem::path frontierSavePath() const;
    [[nodiscard]] std::filesystem::path settingsPath() const;

    HINSTANCE instance_{};
    HWND hwnd_{};
    ViewMode mode_{ViewMode::Hub};
    ViewMode settingsReturnMode_{ViewMode::Hub};
    bool mouseCaptured_{false};
    core::settings::GameSettings settings_{};
    std::unique_ptr<rf::ui::HubPainter> painter_;
    std::unique_ptr<rf::ui::inventory::InventoryPainter> inventoryPainter_;
    std::unique_ptr<rf::ui::menus::PauseMenuPainter> pausePainter_;
    std::unique_ptr<rf::ui::settings::SettingsPainter> settingsPainter_;
    std::unique_ptr<rf::render::VulkanRenderer> renderer_;
};

} // namespace rf::platform

#endif
