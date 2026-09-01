#pragma once

#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"
#include "ui/HubPainter.h"

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
    enum class ViewMode { Hub, VulkanScene };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleAction(rf::ui::HubAction action);
    void enterVulkanScene();
    void returnToHub();

    HINSTANCE instance_{};
    HWND hwnd_{};
    ViewMode mode_{ViewMode::Hub};
    std::unique_ptr<rf::ui::HubPainter> painter_;
    std::unique_ptr<rf::render::VulkanRenderer> renderer_;
};

} // namespace rf::platform

#endif
