#pragma once

#ifdef _WIN32

#include "core/jobs/JobSystem.h"
#include "game/PlayerController.h"
#include "world/FrontierWorld.h"

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
    void onMouseButton(bool primary);
    void setPaused(bool paused) noexcept;
    void saveNow();

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] const std::wstring& lastError() const noexcept { return lastError_; }
    [[nodiscard]] const std::string& gpuName() const noexcept { return gpuName_; }
    [[nodiscard]] std::uint32_t sceneQuadCount() const noexcept { return sceneQuadCount_; }
    [[nodiscard]] std::uint32_t sceneBlockCount() const noexcept { return sceneBlockCount_; }
    [[nodiscard]] world::BlockId selectedBlock() const noexcept { return selectedBlock_; }

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
        BufferResource indices{};
        std::uint32_t indexCount{};
        std::uint32_t quadCount{};
        std::uint32_t solidBlockCount{};
        std::uint64_t revision{};
    };

    struct PendingChunkMesh {
        world::ChunkCoord coord{};
        std::uint64_t revision{};
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
        float pad0{};
        float pad1{};
    };

    static constexpr std::size_t kFramesInFlight = 1;

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
    void updatePushData(float elapsedSeconds);
    void updateWindowTitle();
    void breakTargetBlock();
    void placeTargetBlock();

    void queueDirtyChunkMeshes();
    void pumpChunkMeshJobs();
    void removeUnloadedChunkMeshes();
    bool uploadChunkMesh(world::ChunkCoord coord, std::uint64_t revision, const world::VoxelMesh& mesh,
                         std::uint32_t solidBlockCount);
    void destroyChunkMesh(GpuChunkMesh& mesh);
    void drawSceneMeshes(VkCommandBuffer commandBuffer);
    void refreshSceneCounters();
    [[nodiscard]] bool meshJobPending(world::ChunkCoord coord, std::uint64_t revision) const noexcept;

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
    bool worldMeshDirty_{false};
    unsigned requestedWidth_{1600};
    unsigned requestedHeight_{900};
    std::wstring lastError_;
    std::string gpuName_;

    world::FrontierWorld world_;
    game::PlayerController player_;
    world::BlockId selectedBlock_{world::BlockId::Dirt};

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
    VkPipeline pipeline_{VK_NULL_HANDLE};

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

    std::chrono::steady_clock::time_point startTime_{};
    std::chrono::steady_clock::time_point lastFrameTime_{};
    std::chrono::steady_clock::time_point lastSaveTime_{};
    std::chrono::steady_clock::time_point lastTitleTime_{};
    PushData pushData_{};
};

} // namespace rf::render

#endif
