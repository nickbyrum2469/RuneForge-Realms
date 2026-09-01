#ifdef _WIN32

#include "app/App.h"

#include <objbase.h>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    rf::App app;
    const int code = app.run(instance, showCommand);
    if (SUCCEEDED(hr)) CoUninitialize();
    return code;
}

#endif
