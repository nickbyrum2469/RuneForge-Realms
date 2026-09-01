#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "world/GreedyMesher.h"

#include <array>
#include <cstddef>
#include <fstream>

namespace rf::render {

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
    return std::filesystem::exists(parent) ? parent : direct;
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
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(std::uint32_t);
    info.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    return vkCreateShaderModule(device_, &info, nullptr, &module) == VK_SUCCESS ? module : VK_NULL_HANDLE;
}

bool VulkanRenderer::createPipeline() {
    const auto worldVertexCode = readSpirv(shaderPath(L"voxel_scene.vert.spv"));
    const auto worldFragmentCode = readSpirv(shaderPath(L"voxel_scene.frag.spv"));
    const auto fullscreenVertexCode = readSpirv(shaderPath(L"fullscreen.vert.spv"));
    const auto skyFragmentCode = readSpirv(shaderPath(L"sky.frag.spv"));
    const auto hudFragmentCode = readSpirv(shaderPath(L"hud.frag.spv"));
    if (worldVertexCode.empty() || worldFragmentCode.empty() || fullscreenVertexCode.empty() ||
        skyFragmentCode.empty() || hudFragmentCode.empty()) {
        setError(L"Compiled RuneForge Vulkan visual shaders were not found beside the executable.");
        return false;
    }

    const VkShaderModule worldVertex = createShaderModule(worldVertexCode);
    const VkShaderModule worldFragment = createShaderModule(worldFragmentCode);
    const VkShaderModule fullscreenVertex = createShaderModule(fullscreenVertexCode);
    const VkShaderModule skyFragment = createShaderModule(skyFragmentCode);
    const VkShaderModule hudFragment = createShaderModule(hudFragmentCode);
    if (worldVertex == VK_NULL_HANDLE || worldFragment == VK_NULL_HANDLE || fullscreenVertex == VK_NULL_HANDLE ||
        skyFragment == VK_NULL_HANDLE || hudFragment == VK_NULL_HANDLE) {
        if (worldVertex != VK_NULL_HANDLE) vkDestroyShaderModule(device_, worldVertex, nullptr);
        if (worldFragment != VK_NULL_HANDLE) vkDestroyShaderModule(device_, worldFragment, nullptr);
        if (fullscreenVertex != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fullscreenVertex, nullptr);
        if (skyFragment != VK_NULL_HANDLE) vkDestroyShaderModule(device_, skyFragment, nullptr);
        if (hudFragment != VK_NULL_HANDLE) vkDestroyShaderModule(device_, hudFragment, nullptr);
        setError(L"RuneForge could not create one or more Vulkan shader modules.");
        return false;
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.size = sizeof(PushData);
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    VkResult result = vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device_, hudFragment, nullptr);
        vkDestroyShaderModule(device_, skyFragment, nullptr);
        vkDestroyShaderModule(device_, fullscreenVertex, nullptr);
        vkDestroyShaderModule(device_, worldFragment, nullptr);
        vkDestroyShaderModule(device_, worldVertex, nullptr);
        setError(L"RuneForge pipeline layout creation failed", result);
        return false;
    }

    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

    constexpr std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamic.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo fullscreenVertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(world::MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    const std::array<VkVertexInputAttributeDescription, 3> attributes{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                           static_cast<std::uint32_t>(offsetof(world::MeshVertex, x))},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                           static_cast<std::uint32_t>(offsetof(world::MeshVertex, nx))},
        VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32_UINT,
                                           static_cast<std::uint32_t>(offsetof(world::MeshVertex, material))},
    };
    VkPipelineVertexInputStateCreateInfo worldVertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    worldVertexInput.vertexBindingDescriptionCount = 1;
    worldVertexInput.pVertexBindingDescriptions = &binding;
    worldVertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    worldVertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineDepthStencilStateCreateInfo depthDisabled{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthDisabled.depthTestEnable = VK_FALSE;
    depthDisabled.depthWriteEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthWorld{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthWorld.depthTestEnable = VK_TRUE;
    depthWorld.depthWriteEnable = VK_TRUE;
    depthWorld.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState opaqueAttachment{};
    opaqueAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo opaqueBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    opaqueBlend.attachmentCount = 1;
    opaqueBlend.pAttachments = &opaqueAttachment;

    VkPipelineColorBlendAttachmentState alphaAttachment = opaqueAttachment;
    alphaAttachment.blendEnable = VK_TRUE;
    alphaAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    alphaAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    alphaAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    alphaAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    alphaAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    alphaAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo alphaBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    alphaBlend.attachmentCount = 1;
    alphaBlend.pAttachments = &alphaAttachment;

    auto createGraphics = [&](VkShaderModule vertex, const char* vertexEntry,
                              VkShaderModule fragment, const char* fragmentEntry,
                              VkPipelineVertexInputStateCreateInfo* vertexInput,
                              VkPipelineDepthStencilStateCreateInfo* depth,
                              VkPipelineColorBlendStateCreateInfo* blend,
                              VkPipeline& output) -> VkResult {
        VkPipelineShaderStageCreateInfo vertexStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertex;
        vertexStage.pName = vertexEntry;
        VkPipelineShaderStageCreateInfo fragmentStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragment;
        fragmentStage.pName = fragmentEntry;
        const std::array stages{vertexStage, fragmentStage};

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = static_cast<std::uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = depth;
        info.pColorBlendState = blend;
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout_;
        info.renderPass = renderPass_;
        return vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &output);
    };

    result = createGraphics(fullscreenVertex, "VSFullscreen", skyFragment, "PSSky",
                            &fullscreenVertexInput, &depthDisabled, &opaqueBlend, skyPipeline_);
    if (result == VK_SUCCESS) {
        result = createGraphics(worldVertex, "VSMain", worldFragment, "PSMain",
                                &worldVertexInput, &depthWorld, &opaqueBlend, pipeline_);
    }
    if (result == VK_SUCCESS) {
        result = createGraphics(fullscreenVertex, "VSFullscreen", hudFragment, "PSHud",
                                &fullscreenVertexInput, &depthDisabled, &alphaBlend, hudPipeline_);
    }

    vkDestroyShaderModule(device_, hudFragment, nullptr);
    vkDestroyShaderModule(device_, skyFragment, nullptr);
    vkDestroyShaderModule(device_, fullscreenVertex, nullptr);
    vkDestroyShaderModule(device_, worldFragment, nullptr);
    vkDestroyShaderModule(device_, worldVertex, nullptr);

    if (result != VK_SUCCESS) {
        setError(L"Frontier visual pipeline creation failed", result);
        return false;
    }
    return true;
}

bool VulkanRenderer::createCommandResources() {
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = graphicsQueueFamily_;
    if (vkCreateCommandPool(device_, &pool, nullptr, &commandPool_) != VK_SUCCESS) return false;

    std::array<VkCommandBuffer, kFramesInFlight> buffers{};
    VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocation.commandPool = commandPool_;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = static_cast<std::uint32_t>(buffers.size());
    if (vkAllocateCommandBuffers(device_, &allocation, buffers.data()) != VK_SUCCESS) return false;
    for (std::size_t i = 0; i < buffers.size(); ++i) frames_[i].commandBuffer = buffers[i];
    return true;
}

bool VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (auto& frame : frames_) {
        if (vkCreateSemaphore(device_, &semaphore, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphore, nullptr, &frame.renderFinished) != VK_SUCCESS ||
            vkCreateFence(device_, &fence, nullptr, &frame.inFlight) != VK_SUCCESS) return false;
    }
    return true;
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffer, &begin);
    const std::array<VkClearValue, 2> clearValues{
        VkClearValue{.color = {{0.025f, 0.045f, 0.075f, 1.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}},
    };
    VkRenderPassBeginInfo render{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render.renderPass = renderPass_;
    render.framebuffer = framebuffers_[imageIndex];
    render.renderArea.extent = swapchainExtent_;
    render.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    render.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(commandBuffer, &render, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, swapchainExtent_};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushData), &pushData_);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    drawSceneMeshes(commandBuffer);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline_);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
    vkEndCommandBuffer(commandBuffer);
}

} // namespace rf::render

#endif
