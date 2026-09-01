#include "app/App.h"

#ifdef _WIN32
#include "platform/windows/NativeWindow.h"
#endif

namespace rf {

#ifdef _WIN32
int App::run(HINSTANCE instance, int showCommand) {
    platform::NativeWindow window;
    if (!window.create(instance, showCommand)) return 1;
    return window.run();
}
#else
int App::run() { return 0; }
#endif

} // namespace rf
