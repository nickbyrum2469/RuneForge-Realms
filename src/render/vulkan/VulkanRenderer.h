#pragma once

#ifdef _WIN32

#include "core/jobs/JobSystem.h"
#include "core/settings/GameSettings.h"
#include "game/PlayerController.h"
#include "game/drops/DropSystem.h"
#include "game/inventory/Inventory.h"
#include "game/mining/MiningCadence.h"
#include "game/mining/MiningSystem.h"
#include "game/particles/ParticleSystem.h"
#include "world/FrontierWorld.h"
#include "world/meshing/MicroDetailBuilder.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

#include <volk.h>

namespace rf::render {

class VulkanRenderer {
public:
    VulkanRenderer(HWND hwnd, std::filesystem::path savePath, bool continueExisting);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool initialize();
    void shutdown();
    void drawFrame();
    void resize(unsigned width, unsigned height);
    void onKeyDown(WPARAM key);
    void onKeyUp(WPARAM key);
    void onMouseDelta(float dx, float dy);
    void onMouseButtonDown(bool primary);
    void onMouseButtonUp(bool primary);
    void setPaused(bool paused) noexcept;
    void applySettings(const core::settings::GameSettings& settings) noexcept;
    void saveNow();

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] const std::wstring& lastError() const noexcept { return lastError_; }
    [[nodiscard]] const std::string& gpuName() const noexcept { return gpuName_; }
    [[nodiscard]] std::uint32_t sceneQuadCount() const noexcept { return sceneQuadCount_; }
    [[nodiscard]] std::uint32_t sceneBlockCount() const noexcept { return sceneBlockCount_; }
    [[nodiscard]] const game::inventory::Inventory& inventory() const noexcept { return inventory_; }
    [[nodiscard]] game::mining::MiningMode miningMode() const noexcept { return mining_.mode(); }
    [[nodiscard]] const std::vector<game::drops::WorldDrop>& worldDrops() const noexcept { return drops_.drops(); }
    [[nodiscard]] std::optional<world::BlockId> selectedPlacementBlock() const noexcept;

private:
    struct QueueFamilies {
        std::optional<std::uint32_t> graphics;
        std::optional<std::uint32_t> present;
        [[nodiscard]] bool complete() const noexcept { return graphics.has_value() && present.has_value(); }
    };

    struct SwapchainSupport {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct FrameSync {
        VkSemaphore imageAvailable{VK_NULL_HANDLE};
        VkSemaphore renderFinished{VK_NULL_HANDLE};
        VkFence inFlight{VK_NULL_HANDLE};
        VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    };

    struct BufferResource {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize size{};
    };

    struct GpuChunkMesh {
        BufferResource vertices{};
        BufferResource opaqueIndices{};
        BufferResource waterIndices{};
        std::uint32_t opaqueIndexCount{};
        std::uint32_t waterIndexCount{};
        std::uint32_t quadCount{};
        std::uint32_t solidBlockCount{};
        std::uint64_t revision{};
        world::meshing::SurfaceDetailTier detailTier{world::meshing::SurfaceDetailTier::Distant};
    };

    struct PendingChunkMesh {
        world::ChunkCoord coord{};
        std::uint64_t revision{};
        world::meshing::SurfaceDetailTier detailTier{world::meshing::SurfaceDetailTier::Distant};
        std::future<world::VoxelMesh> future;
    };

    struct PushData {
        float time{};
        float aspect{16.0f / 9.0f};
        float eyeX{};
        float eyeY{};
        float eyeZ{};
        float yaw{};
        float pitch{};
        float viewportWidth{1600.0f};
        float viewportHeight{900.0f};
        float selectedMaterial{};
        float miningMode{};
        float miningProgress{};
        float targetBlockX{};
        float targetBlockY{};
        float targetBlockZ{};
        float targetActive{};
        float fovScale{1.0f};
        std::uint32_t hotbar0{};
        std::uint32_t hotbar1{};
        std::uint32_t hotbar2{};
        std::uint32_t hotbar3{};
        std::uint32_t hotbar4{};
        std::uint32_t hotbar5{};
        std::uint32_t hotbar6{};
        std::uint32_t hotbar7{};
        std::uint32_t hotbar8{};
        std::uint32_t selectedHotbar{};
        float foliageQuality{2.0f};
    };
    static_assert(sizeof(PushData) <= 128, "RuneForge push constants must remain portable");

    static constexpr std::size_t kFramesInFlight = 1;
    static constexpr int kStartupGpuRadius = 1;
    static constexpr int kMeshScheduleBudgetPerFrame = 12;
    static constexpr std::size_t kMaxPendingChunkMeshes = 64;
    static constexpr int kHeroDetailRadius = 2;
    static constexpr int kStandardDetailRadius = 4;

    bool initializeSession();
    bool createInstance();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain();
    bool createImageViews();
    bool createRenderPass();
    bool createPipeline();
    bool createDepthResources();
    bool createFramebuffers();
    bool createCommandResources();
    bool createSyncObjects();
    bool createSceneMesh();

    void destroySwapchainResources();
    void destroySceneMesh();
    bool recreateSwapchain();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void updateGameplay(float deltaSeconds);
    void updateMining(float deltaSeconds);
    void updatePushData(float elapsedSeconds);
    void updateWindowTitle();
    void mineTargetBlock();
    void placeTargetBlock();
    void spawnBlockDrop(world::BlockId block, const world::RaycastHit& hit);

    bool queueChunkMesh(world::ChunkCoord coord);
    void queueDirtyChunkMeshes();
    void pumpChunkMeshJobs();
    void removeUnloadedChunkMeshes();
    void drawSceneMeshes(VkCommandBuffer commandBuffer, bool waterPass);
    void refreshSceneCounters();
    [[nodiscard]] bool meshJobPending(world::ChunkCoord coord, std::uint64_t revision,
                                      world::meshing::SurfaceDetailTier detailTier) const noexcept;
    [[nodiscard]] world::meshing::SurfaceDetailTier surfaceDetailTierFor(world::ChunkCoord coord) const noexcept;
    bool uploadChunkMesh(world::ChunkCoord coord, std::uint64_t revision,
                         world::meshing::SurfaceDetailTier detailTier,
                         const world::VoxelMesh& mesh, std::uint32_t solidBlockCount);
    void destroyChunkMesh(GpuChunkMesh& mesh);

    bool updateDropMesh();
    void drawWorldDrops(VkCommandBuffer commandBuffer);
    void destroyDropMesh();
    bool updateParticleMesh();
    void drawBlockParticles(VkCommandBuffer commandBuffer);
    void destroyParticleMesh();
    bool ensureDynamicBuffer(BufferResource& resource, VkDeviceSize requiredSize, VkBufferUsageFlags usage);
    bool writeDynamicBuffer(BufferResource& resource, const void* data, VkDeviceSize size);

    QueueFamilies findQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const;
    bool deviceSuitable(VkPhysicalDevice device) const;
    bool deviceSupportsRequiredExtensions(VkPhysicalDevice device) const;

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    VkFormat chooseDepthFormat() const;
    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      BufferResource& output);
    bool uploadDeviceLocal(const void* data, VkDeviceSize size, VkBufferUsageFlags finalUsage,
                           BufferResource& output);
    bool copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size);
    void destroyBuffer(BufferResource& resource);

    std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) const;
    std::filesystem::path executableDirectory() const;
    std::filesystem::path shaderPath(const wchar_t* filename) const;
    VkShaderModule createShaderModule(const std::vector<std::uint32_t>& code) const;

    void setError(std::wstring message);
    void setError(const wchar_t* prefix, VkResult result);

    HWND hwnd_{};
    std::filesystem::path savePath_;
    bool continueExisting_{false};
    bool initialized_{false};
    bool sessionReady_{false};
    bool paused_{false};
    bool framebufferResized_{false};
    unsigned requestedWidth_{1600};
    unsigned requestedHeight_{900};
    std::wstring lastError_;
    std::string gpuName_;

    world::FrontierWorld world_;
    game::PlayerController player_;
    game::inventory::Inventory inventory_;
    game::mining::MiningSystem mining_;
    game::mining::MiningCadence miningCadence_;
    game::drops::DropSystem drops_;
    game::particles::ParticleSystem particles_;
    core::settings::GameSettings settings_{};
    std::map<world::BlockId, std::size_t> microHarvestCells_;
    std::optional<world::BlockCoord> currentMiningTarget_;
    float currentMiningProgress_{};

    VkInstance instance_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue graphicsQueue_{VK_NULL_HANDLE};
    VkQueue presentQueue_{VK_NULL_HANDLE};
    std::uint32_t graphicsQueueFamily_{0};
    std::uint32_t presentQueueFamily_{0};

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkFormat swapchainFormat_{VK_FORMAT_UNDEFINED};
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline skyPipeline_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipeline waterPipeline_{VK_NULL_HANDLE};
    VkPipeline hudPipeline_{VK_NULL_HANDLE};

    VkFormat depthFormat_{VK_FORMAT_UNDEFINED};
    VkImage depthImage_{VK_NULL_HANDLE};
    VkDeviceMemory depthMemory_{VK_NULL_HANDLE};
    VkImageView depthImageView_{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool commandPool_{VK_NULL_HANDLE};
    std::array<FrameSync, kFramesInFlight> frames_{};
    std::size_t currentFrame_{0};

    std::map<world::ChunkCoord, GpuChunkMesh> chunkMeshes_;
    std::vector<PendingChunkMesh> pendingChunkMeshes_;
    core::jobs::JobSystem meshJobs_{2};
    std::uint32_t sceneQuadCount_{0};
    std::uint32_t sceneBlockCount_{0};
    std::uint32_t visibleChunkCount_{0};

    BufferResource dropVertices_{};
    BufferResource dropIndices_{};
    std::uint32_t dropIndexCount_{};
    BufferResource particleVertices_{};
    BufferResource particleIndices_{};
    std::uint32_t particleIndexCount_{};

    std::chrono::steady_clock::time_point startTime_{};
    std::chrono::steady_clock::time_point lastFrameTime_{};
    std::chrono::steady_clock::time_point lastSaveTime_{};
    std::chrono::steady_clock::time_point lastTitleTime_{};
    PushData pushData_{};
};

} // namespace rf::render

#endif
