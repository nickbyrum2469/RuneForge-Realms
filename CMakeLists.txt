cmake_minimum_required(VERSION 3.26)
project(RuneForgeRealms VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RF_BUILD_TESTS "Build RuneForge tests" ON)

find_package(Threads REQUIRED)

add_library(runeforge_core STATIC
    src/core/Version.cpp
    src/core/HubModel.cpp
    src/core/jobs/JobSystem.cpp
    src/world/VoxelChunk.cpp
    src/world/GreedyMesher.cpp
    src/world/chunks/ChunkManager.cpp
    src/world/generation/TerrainGenerator.cpp
    src/world/blocks/BlockRegistry.cpp
    src/world/micro/MicroVoxelState.cpp
    src/world/micro/MicroVoxelEdit.cpp
    src/world/meshing/MicroVoxelMesher.cpp
    src/world/meshing/MicroDetailBuilder.cpp
    src/world/growth/GrassGrowth.cpp
    src/world/FrontierWorld.cpp
    src/game/PlayerController.cpp
    src/game/inventory/Inventory.cpp
    src/game/drops/DropSystem.cpp
    src/game/mining/MiningSystem.cpp
    src/render/materials/MaterialRegistry.cpp
    src/render/scene/ChunkCulling.cpp
    src/save/FrontierSave.cpp
    src/save/world/RegionFile.cpp
    src/save/world/MicroRegionFile.cpp
)
target_include_directories(runeforge_core PUBLIC src)
target_compile_definitions(runeforge_core PUBLIC
    RF_VERSION_STRING="${PROJECT_VERSION}"
    RF_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    RF_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    RF_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
target_link_libraries(runeforge_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(runeforge_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(runeforge_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    include(FetchContent)

    FetchContent_Declare(
        rf_vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG v1.4.304
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(
        rf_volk_source
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.4.304
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(rf_vulkan_headers)
    if(NOT rf_vulkan_headers_POPULATED)
        FetchContent_Populate(rf_vulkan_headers)
    endif()
    FetchContent_GetProperties(rf_volk_source)
    if(NOT rf_volk_source_POPULATED)
        FetchContent_Populate(rf_volk_source)
    endif()

    add_library(runeforge_volk STATIC "${rf_volk_source_SOURCE_DIR}/volk.c")
    target_include_directories(runeforge_volk PUBLIC
        "${rf_volk_source_SOURCE_DIR}"
        "${rf_vulkan_headers_SOURCE_DIR}/include"
    )
    target_compile_definitions(runeforge_volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)

    set(RF_DXC_EXECUTABLE "" CACHE FILEPATH "Path to Microsoft's x64 dxc executable")
    set(RF_BUNDLED_DXC "${CMAKE_SOURCE_DIR}/.deps/dxc/bin/x64/dxc.exe")
    if(NOT RF_DXC_EXECUTABLE)
        if(EXISTS "${RF_BUNDLED_DXC}")
            set(RF_DXC_EXECUTABLE "${RF_BUNDLED_DXC}" CACHE FILEPATH "Path to Microsoft's x64 dxc executable" FORCE)
        else()
            find_program(RF_DXC_FROM_PATH NAMES dxc)
            if(RF_DXC_FROM_PATH)
                set(RF_DXC_EXECUTABLE "${RF_DXC_FROM_PATH}" CACHE FILEPATH "Path to Microsoft's x64 dxc executable" FORCE)
            endif()
        endif()
    endif()
    if(NOT RF_DXC_EXECUTABLE OR NOT EXISTS "${RF_DXC_EXECUTABLE}")
        message(FATAL_ERROR "DXC x64 was not found. Run: pwsh -File tools/setup_windows_deps.ps1")
    endif()

    set(RF_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/shaders/voxel_scene.hlsl")
    set(RF_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    set(RF_VERTEX_SPV "${RF_SHADER_OUTPUT_DIR}/voxel_scene.vert.spv")
    set(RF_FRAGMENT_SPV "${RF_SHADER_OUTPUT_DIR}/voxel_scene.frag.spv")
    set(RF_FULLSCREEN_VERTEX_SPV "${RF_SHADER_OUTPUT_DIR}/fullscreen.vert.spv")
    set(RF_SKY_FRAGMENT_SPV "${RF_SHADER_OUTPUT_DIR}/sky.frag.spv")
    set(RF_HUD_FRAGMENT_SPV "${RF_SHADER_OUTPUT_DIR}/hud.frag.spv")

    function(rf_compile_shader output profile entry description)
        add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${RF_SHADER_OUTPUT_DIR}"
            COMMAND "${RF_DXC_EXECUTABLE}" -spirv -T ${profile} -E ${entry}
                    -fspv-target-env=vulkan1.3 -fvk-use-dx-layout
                    -Fo "${output}" "${RF_SHADER_SOURCE}"
            DEPENDS "${RF_SHADER_SOURCE}"
            VERBATIM
            COMMENT "${description}"
        )
    endfunction()

    rf_compile_shader("${RF_VERTEX_SPV}" vs_6_6 VSMain "Compiling RuneForge voxel vertex shader")
    rf_compile_shader("${RF_FRAGMENT_SPV}" ps_6_6 PSMain "Compiling RuneForge hero material shader")
    rf_compile_shader("${RF_FULLSCREEN_VERTEX_SPV}" vs_6_6 VSFullscreen "Compiling RuneForge fullscreen vertex shader")
    rf_compile_shader("${RF_SKY_FRAGMENT_SPV}" ps_6_6 PSSky "Compiling RuneForge procedural sky shader")
    rf_compile_shader("${RF_HUD_FRAGMENT_SPV}" ps_6_6 PSHud "Compiling RuneForge survival HUD shader")

    add_custom_target(runeforge_shaders DEPENDS
        "${RF_VERTEX_SPV}"
        "${RF_FRAGMENT_SPV}"
        "${RF_FULLSCREEN_VERTEX_SPV}"
        "${RF_SKY_FRAGMENT_SPV}"
        "${RF_HUD_FRAGMENT_SPV}"
    )

    add_executable(RuneForgeRealms WIN32
        src/app/App.cpp
        src/app/main_windows.cpp
        src/platform/windows/NativeWindow.cpp
        src/render/vulkan/VulkanRenderer.cpp
        src/render/vulkan/VulkanRendererDevice.cpp
        src/render/vulkan/VulkanRendererSwapchain.cpp
        src/render/vulkan/VulkanRendererPipeline.cpp
        src/render/vulkan/VulkanRendererMesh.cpp
        src/ui/HubLayout.cpp
        src/ui/HubPainter.cpp
        src/ui/inventory/InventoryLayout.cpp
        src/ui/inventory/InventoryPainter.cpp
    )
    add_dependencies(RuneForgeRealms runeforge_shaders)
    target_include_directories(RuneForgeRealms PRIVATE src)
    target_compile_definitions(RuneForgeRealms PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN VK_USE_PLATFORM_WIN32_KHR
    )
    target_link_libraries(RuneForgeRealms PRIVATE
        runeforge_core
        runeforge_volk
        d2d1 dwrite windowscodecs ole32 user32 gdi32 shell32
    )

    add_custom_command(TARGET RuneForgeRealms POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RuneForgeRealms>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RF_VERTEX_SPV}"
                "$<TARGET_FILE_DIR:RuneForgeRealms>/shaders/voxel_scene.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RF_FRAGMENT_SPV}"
                "$<TARGET_FILE_DIR:RuneForgeRealms>/shaders/voxel_scene.frag.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RF_FULLSCREEN_VERTEX_SPV}"
                "$<TARGET_FILE_DIR:RuneForgeRealms>/shaders/fullscreen.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RF_SKY_FRAGMENT_SPV}"
                "$<TARGET_FILE_DIR:RuneForgeRealms>/shaders/sky.frag.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RF_HUD_FRAGMENT_SPV}"
                "$<TARGET_FILE_DIR:RuneForgeRealms>/shaders/hud.frag.spv"
        VERBATIM
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
    install(FILES
        "${RF_VERTEX_SPV}"
        "${RF_FRAGMENT_SPV}"
        "${RF_FULLSCREEN_VERTEX_SPV}"
        "${RF_SKY_FRAGMENT_SPV}"
        "${RF_HUD_FRAGMENT_SPV}"
        DESTINATION runtime/shaders
    )
endif()

if(RF_BUILD_TESTS)
    enable_testing()
    add_executable(runeforge_core_tests
        tests/CoreTests.cpp
        tests/TestCulling.cpp
        tests/TestJobs.cpp
        tests/TestPersistence.cpp
        tests/TestRegistries.cpp
        tests/TestWorld.cpp
        tests/TestMicroMining.cpp
        tests/TestInventoryGrowth.cpp
    )
    target_link_libraries(runeforge_core_tests PRIVATE runeforge_core)
    add_test(NAME runeforge_core_tests COMMAND runeforge_core_tests)
endif()
