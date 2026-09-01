#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

namespace rf {

class App {
public:
#ifdef _WIN32
    int run(HINSTANCE instance, int showCommand);
#else
    int run();
#endif
};

} // namespace rf
