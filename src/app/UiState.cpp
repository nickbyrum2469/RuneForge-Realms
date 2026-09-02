#include "app/UiState.h"

namespace rf::app {

bool UiState::enterGameplay() noexcept {
    if (screen_ != UiScreen::Hub) return false;
    screen_ = UiScreen::Gameplay;
    return true;
}

bool UiState::openPause() noexcept {
    if (screen_ != UiScreen::Gameplay) return false;
    screen_ = UiScreen::Pause;
    return true;
}

bool UiState::closePause() noexcept {
    if (screen_ != UiScreen::Pause) return false;
    screen_ = UiScreen::Gameplay;
    return true;
}

bool UiState::openInventory() noexcept {
    if (screen_ != UiScreen::Gameplay) return false;
    screen_ = UiScreen::Inventory;
    return true;
}

bool UiState::closeInventory() noexcept {
    if (screen_ != UiScreen::Inventory) return false;
    screen_ = UiScreen::Gameplay;
    return true;
}

bool UiState::openSettings() noexcept {
    if (screen_ != UiScreen::Hub && screen_ != UiScreen::Pause) return false;
    settingsReturnScreen_ = screen_;
    screen_ = UiScreen::Settings;
    return true;
}

bool UiState::closeSettings() noexcept {
    if (screen_ != UiScreen::Settings) return false;
    screen_ = settingsReturnScreen_;
    return true;
}

void UiState::returnToHub() noexcept {
    screen_ = UiScreen::Hub;
    settingsReturnScreen_ = UiScreen::Hub;
}

bool UiState::gameplayInputAllowed() const noexcept {
    return screen_ == UiScreen::Gameplay;
}

bool UiState::mouseShouldBeCaptured() const noexcept {
    return screen_ == UiScreen::Gameplay;
}

bool UiState::rendererShouldBePaused() const noexcept {
    return screen_ != UiScreen::Gameplay;
}

bool UiState::nativeOverlayVisible() const noexcept {
    return screen_ == UiScreen::Pause || screen_ == UiScreen::Settings ||
           screen_ == UiScreen::Inventory || screen_ == UiScreen::Crafting;
}

} // namespace rf::app
