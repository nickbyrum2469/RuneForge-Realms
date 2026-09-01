cmake_minimum_required(VERSION 3.26)
project(RuneForgeRealms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RF_BUILD_TESTS "Build RuneForge tests" ON)

add_library(runeforge_core STATIC
    src/core/Version.cpp
    src/core/HubModel.cpp
)
target_include_directories(runeforge_core PUBLIC src)

if(MSVC)
    target_compile_options(runeforge_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(runeforge_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_executable(RuneForgeRealms WIN32
        src/app/App.cpp
        src/app/main_windows.cpp
        src/platform/windows/NativeWindow.cpp
        src/ui/HubLayout.cpp
        src/ui/HubPainter.cpp
    )
    target_include_directories(RuneForgeRealms PRIVATE src)
    target_compile_definitions(RuneForgeRealms PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(RuneForgeRealms PRIVATE
        runeforge_core
        d2d1 dwrite windowscodecs ole32 user32 gdi32 shell32
    )

    add_executable(RuneForgeBootstrap WIN32
        src/updater/BootstrapperMain.cpp
        src/updater/ReleaseClient.cpp
        src/updater/UpdateApplier.cpp
    )
    target_include_directories(RuneForgeBootstrap PRIVATE src)
    target_compile_definitions(RuneForgeBootstrap PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(RuneForgeBootstrap PRIVATE runeforge_core winhttp shell32 ole32)

    install(TARGETS RuneForgeBootstrap RUNTIME DESTINATION .)
    install(TARGETS RuneForgeRealms RUNTIME DESTINATION runtime)
    install(FILES VERSION DESTINATION runtime RENAME version.txt)
endif()

if(RF_BUILD_TESTS)
    enable_testing()
    add_executable(runeforge_core_tests tests/CoreTests.cpp)
    target_link_libraries(runeforge_core_tests PRIVATE runeforge_core)
    add_test(NAME runeforge_core_tests COMMAND runeforge_core_tests)
endif()
