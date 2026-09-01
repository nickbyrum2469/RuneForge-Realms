#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace rf::render {
namespace {
constexpr std::array<const char*, 1> requiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

bool VulkanRenderer::createInstance() {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "RuneForge Realms";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 3, 0);
    appInfo.pEngineName = "RuneForge Native";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const std::array<const char*, 2> extensions{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) { setError(L"vkCreateInstance failed", result); return false; }
    volkLoadInstance(instance_);
    return true;
}

bool VulkanRenderer::createSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    createInfo.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
    createInfo.hwnd = hwnd_;
    const VkResult result = vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_);
    if (result != VK_SUCCESS) { setError(L"vkCreateWin32SurfaceKHR failed", result); return false; }
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
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
        if (present == VK_TRUE) families.present = i;
        if (families.complete()) break;
    }
    return families;
}

bool VulkanRenderer::deviceSupportsRequiredExtensions(VkPhysicalDevice device) const {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
    for (const char* required : requiredDeviceExtensions) {
        bool found = false;
        for (const auto& extension : available) {
            if (std::strcmp(required, extension.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

VulkanRenderer::SwapchainSupport VulkanRenderer::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr);
    support.formats.resize(count);
    if (count) vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, support.formats.data());
    count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr);
    support.presentModes.resize(count);
    if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, support.presentModes.data());
    return support;
}

bool VulkanRenderer::deviceSuitable(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);
    if (VK_API_VERSION_MAJOR(properties.apiVersion) < 1 ||
        (VK_API_VERSION_MAJOR(properties.apiVersion) == 1 && VK_API_VERSION_MINOR(properties.apiVersion) < 3)) return false;
    const auto families = findQueueFamilies(device);
    if (!families.complete() || !deviceSupportsRequiredExtensions(device)) return false;
    const auto swapchain = querySwapchainSupport(device);
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
        if (score > bestScore) { bestScore = score; physicalDevice_ = device; gpuName_ = properties.deviceName; }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        setError(L"RuneForge requires a Vulkan 1.3 GPU/driver with swapchain support.");
        return false;
    }
    return true;
}

bool VulkanRenderer::createDevice() {
    const auto families = findQueueFamilies(physicalDevice_);
    if (!families.complete()) return false;
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
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    const VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) { setError(L"vkCreateDevice failed", result); return false; }

    volkLoadDevice(device_);
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
    return true;
}

std::uint32_t VulkanRenderer::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memory);
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & properties) == properties) return i;
    }
    return std::numeric_limits<std::uint32_t>::max();
}

} // namespace rf::render

#endif
