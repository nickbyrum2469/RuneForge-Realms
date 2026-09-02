#pragma once

namespace rf::app {

enum class UiScreen {
    Hub,
    Gameplay,
    Pause,
    Settings,
    Inventory,
    Crafting,
};

class UiState {
public:
    [[nodiscard]] UiScreen screen() const noexcept { return screen_; }
    [[nodiscard]] UiScreen settingsReturnScreen() const noexcept { return settingsReturnScreen_; }

    [[nodiscard]] bool enterGameplay() noexcept;
    [[nodiscard]] bool openPause() noexcept;
    [[nodiscard]] bool closePause() noexcept;
    [[nodiscard]] bool openInventory() noexcept;
    [[nodiscard]] bool closeInventory() noexcept;
    [[nodiscard]] bool openSettings() noexcept;
    [[nodiscard]] bool closeSettings() noexcept;
    void returnToHub() noexcept;

    [[nodiscard]] bool gameplayInputAllowed() const noexcept;
    [[nodiscard]] bool mouseShouldBeCaptured() const noexcept;
    [[nodiscard]] bool rendererShouldBePaused() const noexcept;
    [[nodiscard]] bool nativeOverlayVisible() const noexcept;

private:
    UiScreen screen_{UiScreen::Hub};
    UiScreen settingsReturnScreen_{UiScreen::Hub};
};

} // namespace rf::app
