#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "save/FrontierSave.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

namespace rf::render {

VulkanRenderer::VulkanRenderer(HWND hwnd, std::filesystem::path savePath, bool continueExisting)
    : hwnd_(hwnd), savePath_(std::move(savePath)), continueExisting_(continueExisting) {}

VulkanRenderer::~VulkanRenderer() { shutdown(); }

bool VulkanRenderer::initializeSession() {
    if (continueExisting_) {
        if (const auto saved = save::loadFrontierSave(savePath_)) {
            world_.generate(saved->seed);
            for (const auto& edit : saved->edits) world_.applyEdit(edit);
            player_.spawn(saved->playerPosition, saved->yaw, saved->pitch);
            world_.updateStreaming(saved->playerPosition.x, saved->playerPosition.z);
            sessionReady_ = true;
            return true;
        }
    }

    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::uint32_t seed = static_cast<std::uint32_t>((ticks >> 17) ^ ticks ^ 0x52465247u);
    world_.generate(seed == 0 ? 1337u : seed);
    const int spawnY = world_.topSolidY(0, 0);
    player_.spawn({0.5f, static_cast<float>(std::max(spawnY + 1, 2)) + 0.01f, 0.5f}, 0.65f, -0.10f);
    sessionReady_ = true;
    return true;
}

bool VulkanRenderer::initialize() {
    if (initialized_) return true;
    lastError_.clear();
    if (!initializeSession()) {
        setError(L"Frontier Realms could not create or load its world session.");
        return false;
    }

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
        !createDepthResources() || !createFramebuffers() || !createCommandResources() ||
        !createSceneMesh() || !createSyncObjects()) {
        shutdown();
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    startTime_ = lastFrameTime_ = lastSaveTime_ = lastTitleTime_ = now;
    initialized_ = true;
    updateWindowTitle();
    return true;
}

void VulkanRenderer::shutdown() {
    if (sessionReady_) saveNow();
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);

    if (device_ != VK_NULL_HANDLE) {
        destroySceneMesh();
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

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;

    physicalDevice_ = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    initialized_ = false;
    currentFrame_ = 0;
}

void VulkanRenderer::saveNow() {
    if (!sessionReady_ || savePath_.empty()) return;
    save::FrontierSaveData data;
    data.seed = world_.seed();
    data.playerPosition = player_.position();
    data.yaw = player_.yaw();
    data.pitch = player_.pitch();
    data.edits = world_.edits();
    save::saveFrontierSave(savePath_, data);
    lastSaveTime_ = std::chrono::steady_clock::now();
}

void VulkanRenderer::updateGameplay(float deltaSeconds) {
    if (paused_) return;
    player_.update(deltaSeconds, world_);
    const auto position = player_.position();
    if (world_.updateStreaming(position.x, position.z)) worldMeshDirty_ = true;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSaveTime_ >= std::chrono::seconds(15)) saveNow();
}

void VulkanRenderer::updatePushData(float elapsedSeconds) {
    const auto eye = player_.eyePosition();
    pushData_.time = elapsedSeconds;
    pushData_.aspect = static_cast<float>(swapchainExtent_.width) /
                       static_cast<float>(std::max<std::uint32_t>(swapchainExtent_.height, 1));
    pushData_.eyeX = eye.x;
    pushData_.eyeY = eye.y;
    pushData_.eyeZ = eye.z;
    pushData_.yaw = player_.yaw();
    pushData_.pitch = player_.pitch();
    pushData_.viewportWidth = static_cast<float>(swapchainExtent_.width);
    pushData_.viewportHeight = static_cast<float>(swapchainExtent_.height);
    pushData_.selectedMaterial = static_cast<float>(static_cast<std::uint32_t>(selectedBlock_));
}

void VulkanRenderer::drawFrame() {
    if (!initialized_ || requestedWidth_ == 0 || requestedHeight_ == 0) return;

    const auto now = std::chrono::steady_clock::now();
    const float delta = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    updateGameplay(delta);

    auto& frame = frames_[currentFrame_];
    vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    if (worldMeshDirty_ && !rebuildSceneMesh()) return;

    std::uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { setError(L"vkAcquireNextImageKHR failed", result); return; }

    const float elapsed = std::chrono::duration<float>(now - startTime_).count();
    updatePushData(elapsed);

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
    if (result != VK_SUCCESS) { setError(L"vkQueueSubmit failed", result); return; }

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) recreateSwapchain();
    else if (result != VK_SUCCESS) setError(L"vkQueuePresentKHR failed", result);

    currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
    if (now - lastTitleTime_ >= std::chrono::milliseconds(250)) {
        updateWindowTitle();
        lastTitleTime_ = now;
    }
}

void VulkanRenderer::resize(unsigned width, unsigned height) {
    requestedWidth_ = width;
    requestedHeight_ = height;
    framebufferResized_ = width > 0 && height > 0;
}

void VulkanRenderer::setPaused(bool paused) noexcept {
    paused_ = paused;
    player_.setControl(game::MoveControl::Forward, false);
    player_.setControl(game::MoveControl::Backward, false);
    player_.setControl(game::MoveControl::Left, false);
    player_.setControl(game::MoveControl::Right, false);
    player_.setControl(game::MoveControl::Sprint, false);
    updateWindowTitle();
}

void VulkanRenderer::onKeyDown(WPARAM key) {
    if (paused_) return;
    switch (key) {
        case 'W': player_.setControl(game::MoveControl::Forward, true); break;
        case 'S': player_.setControl(game::MoveControl::Backward, true); break;
        case 'A': player_.setControl(game::MoveControl::Left, true); break;
        case 'D': player_.setControl(game::MoveControl::Right, true); break;
        case VK_SHIFT: player_.setControl(game::MoveControl::Sprint, true); break;
        case VK_CONTROL: player_.setControl(game::MoveControl::Crouch, true); break;
        case VK_SPACE: player_.requestJump(); break;
        case '1': selectedBlock_ = world::BlockId::Grass; break;
        case '2': selectedBlock_ = world::BlockId::Dirt; break;
        case '3': selectedBlock_ = world::BlockId::Stone; break;
        case '4': selectedBlock_ = world::BlockId::Wood; break;
        case '5': selectedBlock_ = world::BlockId::Leaves; break;
        case VK_F5: saveNow(); break;
        default: break;
    }
}

void VulkanRenderer::onKeyUp(WPARAM key) {
    switch (key) {
        case 'W': player_.setControl(game::MoveControl::Forward, false); break;
        case 'S': player_.setControl(game::MoveControl::Backward, false); break;
        case 'A': player_.setControl(game::MoveControl::Left, false); break;
        case 'D': player_.setControl(game::MoveControl::Right, false); break;
        case VK_SHIFT: player_.setControl(game::MoveControl::Sprint, false); break;
        case VK_CONTROL: player_.setControl(game::MoveControl::Crouch, false); break;
        default: break;
    }
}

void VulkanRenderer::onMouseDelta(float dx, float dy) {
    if (!paused_) player_.addLook(dx, dy);
}

void VulkanRenderer::onMouseButton(bool primary) {
    if (paused_) return;
    if (primary) breakTargetBlock();
    else placeTargetBlock();
}

void VulkanRenderer::breakTargetBlock() {
    const auto eye = player_.eyePosition();
    const auto direction = player_.lookDirection();
    const auto hit = world_.raycast(eye.x, eye.y, eye.z, direction.x, direction.y, direction.z, 6.0f);
    if (!hit.hit || hit.block.y <= 0) return;
    if (world_.setBlock(hit.block.x, hit.block.y, hit.block.z, world::BlockId::Air)) worldMeshDirty_ = true;
}

void VulkanRenderer::placeTargetBlock() {
    const auto eye = player_.eyePosition();
    const auto direction = player_.lookDirection();
    const auto hit = world_.raycast(eye.x, eye.y, eye.z, direction.x, direction.y, direction.z, 6.0f);
    if (!hit.hit || world_.getBlock(hit.adjacent.x, hit.adjacent.y, hit.adjacent.z) != world::BlockId::Air) return;

    const auto feet = player_.position();
    const float halfWidth = player_.bodyWidth() * 0.5f;
    const float px0 = feet.x - halfWidth, px1 = feet.x + halfWidth;
    const float py0 = feet.y, py1 = feet.y + player_.bodyHeight();
    const float pz0 = feet.z - halfWidth, pz1 = feet.z + halfWidth;
    const float bx0 = static_cast<float>(hit.adjacent.x), bx1 = bx0 + 1.0f;
    const float by0 = static_cast<float>(hit.adjacent.y), by1 = by0 + 1.0f;
    const float bz0 = static_cast<float>(hit.adjacent.z), bz1 = bz0 + 1.0f;
    const bool overlapsPlayer = px0 < bx1 && px1 > bx0 && py0 < by1 && py1 > by0 && pz0 < bz1 && pz1 > bz0;
    if (overlapsPlayer) return;

    if (world_.setBlock(hit.adjacent.x, hit.adjacent.y, hit.adjacent.z, selectedBlock_)) worldMeshDirty_ = true;
}

void VulkanRenderer::updateWindowTitle() {
    if (!hwnd_) return;
    const auto position = player_.position();
    const std::string material(world::blockName(selectedBlock_));
    std::wstring selected(material.begin(), material.end());
    std::wstring gpu(gpuName_.begin(), gpuName_.end());
    std::wstring title = L"RuneForge Realms ";
    for (const char* p = RF_VERSION_STRING; *p; ++p) title.push_back(static_cast<wchar_t>(*p));
    title += paused_ ? L" - PAUSED" : L" - Frontier Realms Survival";
    title += L" | Block: " + selected;
    title += L" | XYZ " + std::to_wstring(static_cast<int>(std::floor(position.x))) + L", " +
             std::to_wstring(static_cast<int>(std::floor(position.y))) + L", " +
             std::to_wstring(static_cast<int>(std::floor(position.z)));
    title += L" | Chunks " + std::to_wstring(world_.loadedChunkCount());
    title += L" | " + gpu;
    if (paused_) title += L" | Esc: Resume | H: Save + Main Menu";
    else title += L" | WASD Move | Mouse Look | LMB Break | RMB Place | 1-5 Blocks | Esc Pause";
    SetWindowTextW(hwnd_, title.c_str());
}

void VulkanRenderer::setError(std::wstring message) { lastError_ = std::move(message); }

void VulkanRenderer::setError(const wchar_t* prefix, VkResult result) {
    lastError_ = std::wstring(prefix) + L" (VkResult " + std::to_wstring(static_cast<int>(result)) + L")";
}

} // namespace rf::render

#endif
