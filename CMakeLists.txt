cmake_minimum_required(VERSION 3.26)
project(RuneForgeRealms VERSION 0.5.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RF_BUILD_TESTS "Build RuneForge tests" ON)

find_package(Threads REQUIRED)

add_library(runeforge_core STATIC
    src/app/UiState.cpp
    src/core/Version.cpp
    src/core/HubModel.cpp
    src/core/jobs/JobSystem.cpp
    src/core/settings/GameSettings.cpp
    src/world/VoxelChunk.cpp
    src/world/GreedyMesher.cpp
    src/world/chunks/ChunkManager.cpp
    src/world/generation/TerrainGenerator.cpp
    src/world/blocks/BlockRegistry.cpp
    src/world/fluid/WaterSimulation.cpp
    src/world/micro/MicroVoxelState.cpp
    src/world/micro/MicroVoxelEdit.cpp
    src/world/meshing/MicroVoxelMesher.cpp
    src/world/meshing/MicroDetailBuilder.cpp
    src/world/growth/GrassGrowth.cpp
    src/world/FrontierWorld.cpp
    src/game/PlayerController.cpp
    src/game/character/PlayerBodyRig.cpp
    src/game/items/ItemRegistry.cpp
    src/game/inventory/Inventory.cpp
    src/game/mining/BlockDamageMap.cpp
    src/game/mining/MiningCadence.cpp
    src/game/mining/MiningSystem.cpp
    src/game/interaction/MiningSwing.cpp
    src/game/placement/PlacementSystem.cpp
    src/game/drops/BlockDropSystem.cpp
    src/game/particles/ParticleSystem.cpp
    src/game/audio/AudioEventQueue.cpp
)

target_include_directories(runeforge_core PUBLIC src)
target_compile_definitions(runeforge_core PUBLIC RF_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(runeforge_core PUBLIC Threads::Threads)

if (MSVC)
    target_compile_options(runeforge_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(runeforge_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    find_package(Vulkan REQUIRED)

    add_executable(RuneForgeRealms WIN32
        src/main.cpp
        src/platform/windows/NativeWindow.cpp
        src/platform/windows/WindowsAudioSystem.cpp
        src/render/vulkan/VulkanRenderer.cpp
        src/render/vulkan/VulkanRendererFirstPerson.cpp
        src/render/vulkan/VulkanRendererPipeline.cpp
        src/render/vulkan/VulkanRendererResources.cpp
        src/render/vulkan/VulkanRendererSwapchain.cpp
        src/render/vulkan/VulkanRendererWorld.cpp
        src/render/scene/FirstPersonBodyBuilder.cpp
        src/render/scene/VoxelCharacterBuilder.cpp
        src/render/materials/MaterialRegistry.cpp
        src/ui/HubPainter.cpp
        src/ui/native/NativeUiSurface.cpp
        src/ui/inventory/InventoryPainter.cpp
        src/ui/menus/PauseMenuPainter.cpp
        src/ui/settings/SettingsPainter.cpp
        src/save/FrontierSave.cpp
        src/updater/UpdateNotice.cpp
    )
    target_include_directories(RuneForgeRealms PRIVATE src)
    target_compile_definitions(RuneForgeRealms PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(RuneForgeRealms PRIVATE
        runeforge_core
        Vulkan::Vulkan
        d2d1
        dwrite
        windowscodecs
        winmm
        winhttp
        shell32
        ole32
    )

    add_executable(RuneForgeBootstrap WIN32
        src/updater/BootstrapperMain.cpp
        src/updater/ReleaseClient.cpp
        src/updater/UpdateApplier.cpp
        src/core/Version.cpp
    )
    target_include_directories(RuneForgeBootstrap PRIVATE src)
    target_compile_definitions(RuneForgeBootstrap PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(RuneForgeBootstrap PRIVATE winhttp shell32 ole32)

    find_program(DXC_EXECUTABLE NAMES dxc REQUIRED)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

    function(rf_compile_shader target source entry profile output)
        set(output_path "${CMAKE_CURRENT_BINARY_DIR}/shaders/${output}")
        add_custom_command(
            OUTPUT "${output_path}"
            COMMAND "${DXC_EXECUTABLE}" -spirv -T "${profile}" -E "${entry}" -fspv-target-env=vulkan1.2
                    -fvk-use-dx-layout -Fo "${output_path}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
            COMMENT "Compiling ${source}:${entry} -> ${output}"
            VERBATIM
        )
        add_custom_target("${target}" DEPENDS "${output_path}")
        add_dependencies(RuneForgeRealms "${target}")
    endfunction()

    rf_compile_shader(rf_shader_voxel_vert shaders/voxel_scene.hlsl VSMain vs_6_0 voxel_scene.vert.spv)
    rf_compile_shader(rf_shader_voxel_frag shaders/voxel_scene.hlsl PSMain ps_6_0 voxel_scene.frag.spv)
    rf_compile_shader(rf_shader_character_frag shaders/character_scene.hlsl PSCharacter ps_6_0 character.frag.spv)
    rf_compile_shader(rf_shader_water_frag shaders/water_scene.hlsl PSWater ps_6_0 water.frag.spv)
    rf_compile_shader(rf_shader_fullscreen_vert shaders/voxel_scene.hlsl VSFullscreen vs_6_0 fullscreen.vert.spv)
    rf_compile_shader(rf_shader_sky_frag shaders/voxel_scene.hlsl PSSky ps_6_0 sky.frag.spv)
    rf_compile_shader(rf_shader_hud_frag shaders/voxel_scene.hlsl PSHud ps_6_0 hud.frag.spv)

    install(TARGETS RuneForgeRealms RuneForgeBootstrap RUNTIME DESTINATION .)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders/" DESTINATION shaders)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" DESTINATION runtime RENAME version.txt)
endif()

include(CTest)
if (BUILD_TESTING AND RF_BUILD_TESTS)
    add_executable(runeforge_tests
        tests/TestMain.cpp
        tests/TestVersion.cpp
        tests/TestChunkMeshing.cpp
        tests/TestSettings.cpp
        tests/TestMining.cpp
        tests/TestPlayerController.cpp
        tests/TestPlacement.cpp
        tests/TestFluid.cpp
        tests/TestInventoryGrowth.cpp
        tests/TestCharacterRig.cpp
        tests/TestPolishFoundation.cpp
    )
    target_link_libraries(runeforge_tests PRIVATE runeforge_core)
    add_test(NAME runeforge_tests COMMAND runeforge_tests)
endif()
