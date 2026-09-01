#pragma once

#ifdef _WIN32

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
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleAction(rf::ui::HubAction action);

    HINSTANCE instance_{};
    HWND hwnd_{};
    std::unique_ptr<rf::ui::HubPainter> painter_;
};

} // namespace rf::platform

#endif
