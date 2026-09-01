#pragma once

#ifdef _WIN32

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

#include <volk.h>

namespace rf::render {

class VulkanRenderer {
public:
    explicit VulkanRenderer(HWND hwnd);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool initialize();
    void shutdown();
    void drawFrame();
    void resize(unsigned width, unsigned height);
    void onKeyDown(WPARAM key);

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] const std::wstring& lastError() const noexcept { return lastError_; }
    [[nodiscard]] const std::string& gpuName() const noexcept { return gpuName_; }

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

    struct PushData {
        float time{};
        float aspect{16.0f / 9.0f};
        float yaw{-0.65f};
        float pitch{0.30f};
        float distance{12.5f};
        float pad0{};
        float pad1{};
        float pad2{};
    };

    static constexpr std::size_t kFramesInFlight = 2;
    static constexpr std::uint32_t kSceneInstanceCount = 110;

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

    void destroySwapchainResources();
    bool recreateSwapchain();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);

    QueueFamilies findQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const;
    bool deviceSuitable(VkPhysicalDevice device) const;
    bool deviceSupportsRequiredExtensions(VkPhysicalDevice device) const;

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    VkFormat chooseDepthFormat() const;
    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) const;
    std::filesystem::path executableDirectory() const;
    std::filesystem::path shaderPath(const wchar_t* filename) const;
    VkShaderModule createShaderModule(const std::vector<std::uint32_t>& code) const;

    void setError(std::wstring message);
    void setError(const wchar_t* prefix, VkResult result);

    HWND hwnd_{};
    bool initialized_{false};
    bool framebufferResized_{false};
    unsigned requestedWidth_{1600};
    unsigned requestedHeight_{900};
    std::wstring lastError_;
    std::string gpuName_;

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

    std::chrono::steady_clock::time_point startTime_{};
    PushData pushData_{};
    bool autoOrbit_{true};
};

} // namespace rf::render

#endif
