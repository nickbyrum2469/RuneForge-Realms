#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace rf::render {
namespace {

constexpr std::array<const char*, 1> kRequiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace

VulkanRenderer::VulkanRenderer(HWND hwnd) : hwnd_(hwnd) {}

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

bool VulkanRenderer::initialize() {
    if (initialized_) return true;
    lastError_.clear();

    RECT client{};
    GetClientRect(hwnd_, &client);
    requestedWidth_ = static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1));
    requestedHeight_ = static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1));

    if (volkInitialize() != VK_SUCCESS) {
        setError(L"Vulkan loader initialization failed. Install a current GPU driver with Vulkan support.");
        return false;
    }

    if (!createInstance() || !createSurface() || !pickPhysicalDevice() || !createDevice() ||
        !createSwapchain() || !createImageViews() || !createRenderPass() || !createPipeline() ||
        !createDepthResources() || !createFramebuffers() || !createCommandResources() || !createSyncObjects()) {
        shutdown();
        return false;
    }

    startTime_ = std::chrono::steady_clock::now();
    initialized_ = true;
    return true;
}

void VulkanRenderer::shutdown() {
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);

    if (device_ != VK_NULL_HANDLE) {
        for (auto& frame : frames_) {
            if (frame.imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
            if (frame.renderFinished != VK_NULL_HANDLE) vkDestroySemaphore(device_, frame.renderFinished, nullptr);
            if (frame.inFlight != VK_NULL_HANDLE) vkDestroyFence(device_, frame.inFlight, nullptr);
            frame = {};
        }
        if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
        destroySwapchainResources();
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    physicalDevice_ = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    initialized_ = false;
    currentFrame_ = 0;
}

bool VulkanRenderer::createInstance() {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "RuneForge Realms";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 2, 0);
    appInfo.pEngineName = "RuneForge Native";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 2, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const std::array<const char*, 2> extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateInstance failed", result);
        return false;
    }

    volkLoadInstance(instance_);
    return true;
}

bool VulkanRenderer::createSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    createInfo.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
    createInfo.hwnd = hwnd_;

    const VkResult result = vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateWin32SurfaceKHR failed", result);
        return false;
    }
    return true;
}

VulkanRenderer::QueueFamilies VulkanRenderer::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilies families;
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

    for (std::uint32_t i = 0; i < count; ++i) {
        if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) families.graphics = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (presentSupport == VK_TRUE) families.present = i;
        if (families.complete()) break;
    }
    return families;
}

bool VulkanRenderer::deviceSupportsRequiredExtensions(VkPhysicalDevice device) const {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (const char* required : kRequiredDeviceExtensions) {
        bool found = false;
        for (const auto& extension : available) {
            if (std::strcmp(required, extension.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

VulkanRenderer::SwapchainSupport VulkanRenderer::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0) vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, support.formats.data());

    std::uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentCount, nullptr);
    support.presentModes.resize(presentCount);
    if (presentCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentCount, support.presentModes.data());
    }
    return support;
}

bool VulkanRenderer::deviceSuitable(VkPhysicalDevice device) const {
    const auto properties = [&]() {
        VkPhysicalDeviceProperties value{};
        vkGetPhysicalDeviceProperties(device, &value);
        return value;
    }();
    if (VK_API_VERSION_MAJOR(properties.apiVersion) < 1 ||
        (VK_API_VERSION_MAJOR(properties.apiVersion) == 1 && VK_API_VERSION_MINOR(properties.apiVersion) < 3)) {
        return false;
    }

    const QueueFamilies families = findQueueFamilies(device);
    if (!families.complete() || !deviceSupportsRequiredExtensions(device)) return false;
    const SwapchainSupport swapchain = querySwapchainSupport(device);
    return !swapchain.formats.empty() && !swapchain.presentModes.empty();
}

bool VulkanRenderer::pickPhysicalDevice() {
    std::uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        setError(L"No Vulkan-capable GPU was found. Update the GPU driver and try again.");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    int bestScore = -1;
    for (VkPhysicalDevice device : devices) {
        if (!deviceSuitable(device)) continue;
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        int score = static_cast<int>(properties.limits.maxImageDimension2D);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 100000;
        if (score > bestScore) {
            bestScore = score;
            physicalDevice_ = device;
            gpuName_ = properties.deviceName;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        setError(L"RuneForge requires a Vulkan 1.3 GPU/driver with swapchain support. No compatible device was found.");
        return false;
    }
    return true;
}

bool VulkanRenderer::createDevice() {
    const QueueFamilies families = findQueueFamilies(physicalDevice_);
    if (!families.complete()) {
        setError(L"Selected Vulkan GPU lost its required graphics/present queue support.");
        return false;
    }
    graphicsQueueFamily_ = *families.graphics;
    presentQueueFamily_ = *families.present;

    const float priority = 1.0f;
    std::array<std::uint32_t, 2> familyIndices{graphicsQueueFamily_, presentQueueFamily_};
    const std::size_t queueCount = graphicsQueueFamily_ == presentQueueFamily_ ? 1 : 2;
    std::array<VkDeviceQueueCreateInfo, 2> queues{};
    for (std::size_t i = 0; i < queueCount; ++i) {
        queues[i] = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queues[i].queueFamilyIndex = familyIndices[i];
        queues[i].queueCount = 1;
        queues[i].pQueuePriorities = &priority;
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCount);
    createInfo.pQueueCreateInfos = queues.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(kRequiredDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kRequiredDeviceExtensions.data();

    const VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateDevice failed", result);
        return false;
    }

    volkLoadDevice(device_);
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
    return true;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR VulkanRenderer::choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    for (VkPresentModeKHR mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    VkExtent2D extent{requestedWidth_, requestedHeight_};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

bool VulkanRenderer::createSwapchain() {
    const SwapchainSupport support = querySwapchainSupport(physicalDevice_);
    if (support.formats.empty() || support.presentModes.empty()) {
        setError(L"The selected GPU no longer reports usable surface formats/present modes.");
        return false;
    }

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    const VkExtent2D extent = chooseExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((support.capabilities.supportedCompositeAlpha & compositeAlpha) == 0) {
        constexpr std::array<VkCompositeAlphaFlagBitsKHR, 3> alternatives{
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        for (auto candidate : alternatives) {
            if ((support.capabilities.supportedCompositeAlpha & candidate) != 0) {
                compositeAlpha = candidate;
                break;
            }
        }
    }

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::array<std::uint32_t, 2> queueFamilies{graphicsQueueFamily_, presentQueueFamily_};
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilies.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    const VkResult result = vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateSwapchainKHR failed", result);
        return false;
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
    return true;
}

bool VulkanRenderer::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = swapchainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat_;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        const VkResult result = vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]);
        if (result != VK_SUCCESS) {
            setError(L"vkCreateImageView (swapchain) failed", result);
            return false;
        }
    }
    return true;
}

VkFormat VulkanRenderer::chooseDepthFormat() const {
    constexpr std::array<VkFormat, 3> candidates{
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) return format;
    }
    return VK_FORMAT_UNDEFINED;
}

bool VulkanRenderer::createRenderPass() {
    depthFormat_ = chooseDepthFormat();
    if (depthFormat_ == VK_FORMAT_UNDEFINED) {
        setError(L"No compatible Vulkan depth-buffer format was found.");
        return false;
    }

    VkAttachmentDescription color{};
    color.format = swapchainFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = depthFormat_;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 2> attachments{color, depth};
    VkRenderPassCreateInfo createInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    const VkResult result = vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateRenderPass failed", result);
        return false;
    }
    return true;
}

std::filesystem::path VulkanRenderer::executableDirectory() const {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path VulkanRenderer::shaderPath(const wchar_t* filename) const {
    const auto executable = executableDirectory();
    const auto direct = executable / L"shaders" / filename;
    if (std::filesystem::exists(direct)) return direct;
    const auto parent = executable.parent_path() / L"shaders" / filename;
    if (std::filesystem::exists(parent)) return parent;
    return direct;
}

std::vector<std::uint32_t> VulkanRenderer::readSpirv(const std::filesystem::path& path) const {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const std::streamsize size = input.tellg();
    if (size <= 0 || (size % 4) != 0) return {};
    input.seekg(0, std::ios::beg);
    std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
    if (!input.read(reinterpret_cast<char*>(words.data()), size)) return {};
    return words;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<std::uint32_t>& code) const {
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size() * sizeof(std::uint32_t);
    createInfo.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
    return module;
}

bool VulkanRenderer::createPipeline() {
    const auto vertexCode = readSpirv(shaderPath(L"voxel_scene.vert.spv"));
    const auto fragmentCode = readSpirv(shaderPath(L"voxel_scene.frag.spv"));
    if (vertexCode.empty() || fragmentCode.empty()) {
        setError(L"Compiled RuneForge Vulkan shaders were not found beside the executable.");
        return false;
    }

    const VkShaderModule vertexModule = createShaderModule(vertexCode);
    const VkShaderModule fragmentModule = createShaderModule(fragmentCode);
    if (vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE) {
        if (vertexModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertexModule, nullptr);
        if (fragmentModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragmentModule, nullptr);
        setError(L"Vulkan shader-module creation failed.");
        return false;
    }

    VkPipelineShaderStageCreateInfo vertexStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexModule;
    vertexStage.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragmentStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentModule;
    fragmentStage.pName = "PSMain";

    const std::array<VkPipelineShaderStageCreateInfo, 2> stages{vertexStage, fragmentStage};

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorAttachment{};
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &colorAttachment;

    constexpr std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamic.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushData);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    VkResult result = vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        setError(L"vkCreatePipelineLayout failed", result);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depth;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateGraphicsPipelines failed", result);
        return false;
    }
    return true;
}

std::uint32_t VulkanRenderer::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        const bool typeAllowed = (typeFilter & (1u << i)) != 0;
        const bool flagsMatch = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeAllowed && flagsMatch) return i;
    }
    return std::numeric_limits<std::uint32_t>::max();
}

bool VulkanRenderer::createDepthResources() {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(device_, &imageInfo, nullptr, &depthImage_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateImage (depth) failed", result);
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, depthImage_, &requirements);
    const std::uint32_t memoryType = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryType == std::numeric_limits<std::uint32_t>::max()) {
        setError(L"No device-local Vulkan memory type was available for the depth buffer.");
        return false;
    }

    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(device_, &allocateInfo, nullptr, &depthMemory_);
    if (result != VK_SUCCESS) {
        setError(L"vkAllocateMemory (depth) failed", result);
        return false;
    }
    result = vkBindImageMemory(device_, depthImage_, depthMemory_, 0);
    if (result != VK_SUCCESS) {
        setError(L"vkBindImageMemory (depth) failed", result);
        return false;
    }

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (hasStencilComponent(depthFormat_)) viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateImageView (depth) failed", result);
        return false;
    }
    return true;
}

bool VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        const std::array<VkImageView, 2> attachments{swapchainImageViews_[i], depthImageView_};
        VkFramebufferCreateInfo createInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = renderPass_;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        const VkResult result = vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            setError(L"vkCreateFramebuffer failed", result);
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::createCommandResources() {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily_;
    VkResult result = vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);
    if (result != VK_SUCCESS) {
        setError(L"vkCreateCommandPool failed", result);
        return false;
    }

    std::array<VkCommandBuffer, kFramesInFlight> commandBuffers{};
    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());
    result = vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers.data());
    if (result != VK_SUCCESS) {
        setError(L"vkAllocateCommandBuffers failed", result);
        return false;
    }
    for (std::size_t i = 0; i < kFramesInFlight; ++i) frames_[i].commandBuffer = commandBuffers[i];
    return true;
}

bool VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& frame : frames_) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.renderFinished) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS) {
            setError(L"Vulkan frame synchronization object creation failed.");
            return false;
        }
    }
    return true;
}

void VulkanRenderer::destroySwapchainResources() {
    if (device_ == VK_NULL_HANDLE) return;

    for (VkFramebuffer framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();

    if (depthImageView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, depthImageView_, nullptr);
    if (depthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, depthImage_, nullptr);
    if (depthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, depthMemory_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    depthImage_ = VK_NULL_HANDLE;
    depthMemory_ = VK_NULL_HANDLE;

    if (pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    renderPass_ = VK_NULL_HANDLE;

    for (VkImageView view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

bool VulkanRenderer::recreateSwapchain() {
    if (requestedWidth_ == 0 || requestedHeight_ == 0) return false;
    vkDeviceWaitIdle(device_);
    destroySwapchainResources();
    framebufferResized_ = false;
    return createSwapchain() && createImageViews() && createRenderPass() && createPipeline() &&
           createDepthResources() && createFramebuffers();
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    const std::array<VkClearValue, 2> clearValues{
        VkClearValue{.color = {{0.018f, 0.034f, 0.075f, 1.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}},
    };

    VkRenderPassBeginInfo renderInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderInfo.renderPass = renderPass_;
    renderInfo.framebuffer = framebuffers_[imageIndex];
    renderInfo.renderArea.offset = {0, 0};
    renderInfo.renderArea.extent = swapchainExtent_;
    renderInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swapchainExtent_};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushData), &pushData_);
    vkCmdDraw(commandBuffer, 36, kSceneInstanceCount, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
    vkEndCommandBuffer(commandBuffer);
}

void VulkanRenderer::drawFrame() {
    if (!initialized_ || requestedWidth_ == 0 || requestedHeight_ == 0) return;

    auto& frame = frames_[currentFrame_];
    vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        setError(L"vkAcquireNextImageKHR failed", result);
        return;
    }

    const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();
    pushData_.time = seconds;
    pushData_.aspect = static_cast<float>(swapchainExtent_.width) /
                       static_cast<float>(std::max<std::uint32_t>(swapchainExtent_.height, 1));
    if (autoOrbit_) pushData_.yaw = -0.65f + seconds * 0.16f;

    vkResetFences(device_, 1, &frame.inFlight);
    vkResetCommandBuffer(frame.commandBuffer, 0);
    recordCommandBuffer(frame.commandBuffer, imageIndex);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinished;

    result = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, frame.inFlight);
    if (result != VK_SUCCESS) {
        setError(L"vkQueueSubmit failed", result);
        return;
    }

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(presentQueue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        setError(L"vkQueuePresentKHR failed", result);
    }

    currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
}

void VulkanRenderer::resize(unsigned width, unsigned height) {
    requestedWidth_ = width;
    requestedHeight_ = height;
    framebufferResized_ = width > 0 && height > 0;
}

void VulkanRenderer::onKeyDown(WPARAM key) {
    switch (key) {
        case VK_LEFT:
            autoOrbit_ = false;
            pushData_.yaw -= 0.12f;
            break;
        case VK_RIGHT:
            autoOrbit_ = false;
            pushData_.yaw += 0.12f;
            break;
        case VK_UP:
            pushData_.pitch = std::clamp(pushData_.pitch + 0.08f, -0.8f, 0.9f);
            break;
        case VK_DOWN:
            pushData_.pitch = std::clamp(pushData_.pitch - 0.08f, -0.8f, 0.9f);
            break;
        case 'W':
        case VK_OEM_PLUS:
            pushData_.distance = std::max(5.0f, pushData_.distance - 0.7f);
            break;
        case 'S':
        case VK_OEM_MINUS:
            pushData_.distance = std::min(30.0f, pushData_.distance + 0.7f);
            break;
        case VK_SPACE:
            autoOrbit_ = !autoOrbit_;
            break;
        case 'R':
            pushData_.yaw = -0.65f;
            pushData_.pitch = 0.30f;
            pushData_.distance = 12.5f;
            autoOrbit_ = true;
            startTime_ = std::chrono::steady_clock::now();
            break;
        default:
            break;
    }
}

void VulkanRenderer::setError(std::wstring message) {
    lastError_ = std::move(message);
}

void VulkanRenderer::setError(const wchar_t* prefix, VkResult result) {
    lastError_ = std::wstring(prefix) + L" (VkResult " + std::to_wstring(static_cast<int>(result)) + L")";
}

} // namespace rf::render

#endif
