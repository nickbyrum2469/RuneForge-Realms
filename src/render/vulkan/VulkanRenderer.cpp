#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "game/items/ItemId.h"
#include "save/FrontierSave.h"
#include "world/blocks/BlockRegistry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

namespace rf::render {
namespace {

std::uint32_t packHotbarStack(const game::inventory::ItemStack& stack) noexcept {
    if (stack.empty()) return 0;
    return (static_cast<std::uint32_t>(stack.item) & 0xffu) |
           ((static_cast<std::uint32_t>(stack.count) & 0xffu) << 8u);
}

struct SpawnPoint {
    int x{};
    int y{};
    int z{};
};

SpawnPoint findDrySpawn(const world::FrontierWorld& world) noexcept {
    constexpr int maxRadius = 40;
    for (int radius = 0; radius <= maxRadius; ++radius) {
        for (int z = -radius; z <= radius; ++z) {
            for (int x = -radius; x <= radius; ++x) {
                if (radius > 0 && std::abs(x) != radius && std::abs(z) != radius) continue;
                const int ground = world.topSolidY(x, z);
                if (ground < 1 || ground + 2 >= world::VoxelChunk::sizeY) continue;
                if (world.getBlock(x, ground, z) != world::BlockId::Grass) continue;
                if (world.getBlock(x, ground + 1, z) != world::BlockId::Air) continue;
                if (world.getBlock(x, ground + 2, z) != world::BlockId::Air) continue;
                return {x, ground + 1, z};
            }
        }
    }

    const int fallback = std::max(world.topSolidY(0, 0) + 1, 2);
    return {0, fallback, 0};
}

game::Vec3 cameraRight(game::Vec3 forward) noexcept {
    game::Vec3 right = game::normalized({forward.z, 0.0f, -forward.x});
    if (game::lengthSquared(right) <= 0.000001f) right = {1.0f, 0.0f, 0.0f};
    return right;
}

game::Vec3 cameraUp(game::Vec3 forward, game::Vec3 right) noexcept {
    // cross(forward, right). Keeping this in one helper prevents first-person rendering and
    // physical contact from silently disagreeing about the player's camera basis.
    return game::normalized({
        -forward.y * right.z,
        forward.z * right.x - forward.x * right.z,
        -forward.y * right.x,
    });
}

} // namespace

VulkanRenderer::VulkanRenderer(HWND hwnd, std::filesystem::path savePath, bool continueExisting)
    : hwnd_(hwnd), savePath_(std::move(savePath)), continueExisting_(continueExisting) {}

VulkanRenderer::~VulkanRenderer() { shutdown(); }

bool VulkanRenderer::initializeSession() {
    inventory_.clear();
    drops_.clear();
    particles_.clear();
    audioEvents_.clear();
    mining_.clearAllDamage();
    mining_.setMode(game::mining::MiningMode::Mixed);
    miningCadence_.reset();
    miningSwing_.reset();
    microHarvestCells_.clear();
    currentMiningTarget_.reset();
    currentMiningProgress_ = 0.0f;

    if (continueExisting_) {
        if (const auto saved = save::loadFrontierSave(savePath_)) {
            world_.generate(saved->seed);
            world_.setWorldAgeSeconds(saved->worldAgeSeconds);
            for (const auto& edit : saved->edits) world_.applyEdit(edit);
            for (const auto& edit : saved->microEdits) world_.applyMicroEdit(edit);
            inventory_ = saved->inventory;
            mining_.setMode(saved->miningMode);
            mining_.restoreDamage(saved->miningDamage);
            for (std::size_t block = 1; block < saved->microHarvestCells.size(); ++block) {
                if (saved->microHarvestCells[block] == 0) continue;
                microHarvestCells_[static_cast<world::BlockId>(block)] = saved->microHarvestCells[block];
            }
            drops_.restore(saved->drops);
            player_.spawn(saved->playerPosition, saved->yaw, saved->pitch);
            player_.setMouseSensitivity(settings_.mouseSensitivity);
            world_.updateStreaming(saved->playerPosition.x, saved->playerPosition.z);
            sessionReady_ = true;
            return true;
        }
    }

    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::uint32_t seed = static_cast<std::uint32_t>((ticks >> 17) ^ ticks ^ 0x52465247u);
    world_.generate(seed == 0 ? 1337u : seed);
    const SpawnPoint spawn = findDrySpawn(world_);
    player_.spawn({static_cast<float>(spawn.x) + 0.5f,
                   static_cast<float>(spawn.y) + 0.01f,
                   static_cast<float>(spawn.z) + 0.5f}, 0.65f, -0.10f);
    player_.setMouseSensitivity(settings_.mouseSensitivity);
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
    meshJobs_.waitIdle();
    pendingChunkMeshes_.clear();
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);

    if (device_ != VK_NULL_HANDLE) {
        destroyFirstPersonBodyMesh();
        destroyParticleMesh();
        destroyDropMesh();
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
    sessionReady_ = false;
    currentFrame_ = 0;
}

void VulkanRenderer::applySettings(const core::settings::GameSettings& source) noexcept {
    settings_ = source;
    settings_.sanitize();
    player_.setMouseSensitivity(settings_.mouseSensitivity);
}

void VulkanRenderer::saveNow() {
    if (!sessionReady_ || savePath_.empty()) return;
    save::FrontierSaveData data;
    data.seed = world_.seed();
    data.worldAgeSeconds = world_.worldAgeSeconds();
    data.playerPosition = player_.position();
    data.yaw = player_.yaw();
    data.pitch = player_.pitch();
    data.miningMode = mining_.mode();
    data.inventory = inventory_;
    data.miningDamage = mining_.damageStates();
    for (const auto& [block, cells] : microHarvestCells_) {
        const std::size_t index = static_cast<std::size_t>(block);
        if (index >= data.microHarvestCells.size()) continue;
        data.microHarvestCells[index] = static_cast<std::uint32_t>(cells % world::micro::cellCount);
    }
    data.drops = drops_.drops();
    data.edits = world_.edits();
    data.microEdits = world_.microEdits();
    (void)save::saveFrontierSave(savePath_, data);
    lastSaveTime_ = std::chrono::steady_clock::now();
}

void VulkanRenderer::updateMining(float deltaSeconds) {
    const auto eye = player_.eyePosition();
    const auto forward = player_.lookDirection();
    const auto right = cameraRight(forward);
    const auto up = cameraUp(forward, right);

    // The view ray only chooses what the player intends to punch. It cannot apply damage by itself.
    // Reach is capped to the fist's physical envelope; a six-metre camera laser can no longer mine.
    constexpr float acquisitionReach = game::interaction::MiningSwing::fistReach + 0.04f;
    const auto target = world_.raycast(eye.x, eye.y, eye.z,
                                       forward.x, forward.y, forward.z, acquisitionReach);
    const world::BlockId block = target.hit
        ? world_.getBlock(target.block.x, target.block.y, target.block.z)
        : world::BlockId::Air;
    const float interval = block == world::BlockId::Air
        ? 0.45f
        : game::mining::MiningSystem::strikeInterval(block);

    if (!miningSwing_.active() && target.hit && target.block.y > 0 &&
        miningCadence_.update(deltaSeconds, interval)) {
        // Contact happens during the middle of this animation. Recovery remains controlled by the
        // independent mining cadence, so button spam cannot start overlapping arms or skip cooldowns.
        const float swingDuration = std::clamp(interval * 0.90f, 0.38f, 0.92f);
        (void)miningSwing_.begin(target, eye, forward, right, up, swingDuration);
    } else if (!miningSwing_.active()) {
        // Keep cooldown time moving even while no block is reachable, without fabricating a swing.
        (void)miningCadence_.update(deltaSeconds, interval);
    }

    if (miningSwing_.active()) {
        if (const auto contact = miningSwing_.update(deltaSeconds, world_, eye, forward, right, up)) {
            applyMiningContact(contact->hit);
        }
    }
}

void VulkanRenderer::updateGameplay(float deltaSeconds) {
    if (paused_) return;
    player_.update(deltaSeconds, world_);
    const auto position = player_.position();
    (void)world_.updateStreaming(position.x, position.z);
    (void)world_.advanceSimulation(deltaSeconds);
    updateMining(deltaSeconds);
    drops_.update(deltaSeconds, world_, position, inventory_);
    particles_.update(deltaSeconds, world_);

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSaveTime_ >= std::chrono::seconds(15)) saveNow();
}

std::optional<world::BlockId> VulkanRenderer::selectedPlacementBlock() const noexcept {
    const auto& stack = inventory_.selectedStack();
    if (stack.empty()) return std::nullopt;
    return game::items::blockForItem(stack.item);
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
    const float fovRadians = settings_.fovDegrees * 0.01745329251994329577f;
    pushData_.fovScale = 1.0f / std::tan(fovRadians * 0.5f);
    pushData_.foliageQuality = static_cast<float>(settings_.foliageQuality);

    const auto selected = selectedPlacementBlock();
    pushData_.selectedMaterial = selected ? static_cast<float>(static_cast<std::uint32_t>(*selected)) : -1.0f;
    pushData_.miningMode = static_cast<float>(static_cast<std::uint8_t>(mining_.mode()));

    const auto direction = player_.lookDirection();
    constexpr float interactionReach = game::interaction::MiningSwing::fistReach + 0.04f;
    const auto hit = world_.raycast(eye.x, eye.y, eye.z,
                                    direction.x, direction.y, direction.z, interactionReach);
    if (hit.hit && hit.block.y > 0) {
        currentMiningTarget_ = hit.block;
        if (mining_.mode() == game::mining::MiningMode::Micro) {
            const auto* state = world_.microState(hit.block);
            currentMiningProgress_ = state ? (1.0f - state->solidFraction()) : 0.0f;
        } else {
            currentMiningProgress_ = mining_.damageAt(hit.block);
        }
        pushData_.targetBlockX = static_cast<float>(hit.block.x);
        pushData_.targetBlockY = static_cast<float>(hit.block.y);
        pushData_.targetBlockZ = static_cast<float>(hit.block.z);
        pushData_.targetActive = 1.0f;
    } else {
        currentMiningTarget_.reset();
        currentMiningProgress_ = 0.0f;
        pushData_.targetBlockX = pushData_.targetBlockY = pushData_.targetBlockZ = 0.0f;
        pushData_.targetActive = 0.0f;
    }
    pushData_.miningProgress = currentMiningProgress_;

    const auto& slots = inventory_.slots();
    pushData_.hotbar0 = packHotbarStack(slots[0]);
    pushData_.hotbar1 = packHotbarStack(slots[1]);
    pushData_.hotbar2 = packHotbarStack(slots[2]);
    pushData_.hotbar3 = packHotbarStack(slots[3]);
    pushData_.hotbar4 = packHotbarStack(slots[4]);
    pushData_.hotbar5 = packHotbarStack(slots[5]);
    pushData_.hotbar6 = packHotbarStack(slots[6]);
    pushData_.hotbar7 = packHotbarStack(slots[7]);
    pushData_.hotbar8 = packHotbarStack(slots[8]);
    pushData_.selectedHotbar = static_cast<std::uint32_t>(inventory_.selectedHotbar());
}

void VulkanRenderer::drawFrame() {
    if (!initialized_ || requestedWidth_ == 0 || requestedHeight_ == 0) return;

    const auto now = std::chrono::steady_clock::now();
    const float delta = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    updateGameplay(delta);

    auto& frame = frames_[currentFrame_];
    vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    removeUnloadedChunkMeshes();
    queueDirtyChunkMeshes();
    pumpChunkMeshJobs();
    if (!updateDropMesh() || !updateParticleMesh() || !updateFirstPersonBodyMesh()) return;

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
    miningCadence_.release();
    if (paused) miningSwing_.reset();
    player_.setControl(game::MoveControl::Forward, false);
    player_.setControl(game::MoveControl::Backward, false);
    player_.setControl(game::MoveControl::Left, false);
    player_.setControl(game::MoveControl::Right, false);
    player_.setControl(game::MoveControl::Sprint, false);
    player_.setControl(game::MoveControl::Crouch, false);
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
        case '1': inventory_.selectHotbar(0); break;
        case '2': inventory_.selectHotbar(1); break;
        case '3': inventory_.selectHotbar(2); break;
        case '4': inventory_.selectHotbar(3); break;
        case '5': inventory_.selectHotbar(4); break;
        case '6': inventory_.selectHotbar(5); break;
        case '7': inventory_.selectHotbar(6); break;
        case '8': inventory_.selectHotbar(7); break;
        case '9': inventory_.selectHotbar(8); break;
        case 'M': mining_.cycleMode(); currentMiningProgress_ = 0.0f; miningSwing_.reset(); break;
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

void VulkanRenderer::onMouseButtonDown(bool primary) {
    if (paused_) return;
    if (primary) miningCadence_.press();
    else placeTargetBlock();
}

void VulkanRenderer::onMouseButtonUp(bool primary) {
    if (primary) miningCadence_.release();
}

void VulkanRenderer::spawnBlockDrop(world::BlockId block, const world::RaycastHit& hit) {
    const auto item = game::items::itemForBlock(block);
    if (!item) return;
    const auto direction = player_.lookDirection();
    drops_.spawn(*item, 1,
                 {static_cast<float>(hit.block.x) + 0.5f,
                  static_cast<float>(hit.block.y) + 0.65f,
                  static_cast<float>(hit.block.z) + 0.5f},
                 {direction.x * 0.8f, 2.2f, direction.z * 0.8f});
}

void VulkanRenderer::applyMiningContact(const world::RaycastHit& hit) {
    if (!hit.hit || hit.block.y <= 0) return;
    const world::BlockId before = world_.getBlock(hit.block.x, hit.block.y, hit.block.z);
    if (!world::isSolid(before)) return;
    const auto& definition = world::blocks::BlockRegistry::get(before);

    if (!currentMiningTarget_ || *currentMiningTarget_ != hit.block) {
        currentMiningTarget_ = hit.block;
        currentMiningProgress_ = mining_.damageAt(hit.block);
    }

    const auto outcome = mining_.strike(world_, hit);
    currentMiningProgress_ = outcome.damageProgress;
    if (!outcome.affected) return;

    world_.markBlockVisualDirty(hit.block);
    particles_.emitBlockBurst(outcome.block,
                              {hit.worldX, hit.worldY, hit.worldZ},
                              outcome.brokeBlock ? 13u : 3u,
                              outcome.brokeBlock ? 1.25f : 0.65f);
    audioEvents_.emitBlock(outcome.brokeBlock ? game::audio::AudioEventType::BlockBreak
                                             : game::audio::AudioEventType::MiningHit,
                           definition.soundFamily,
                           {hit.worldX, hit.worldY, hit.worldZ},
                           outcome.brokeBlock ? 1.0f : 0.72f, true);

    if (mining_.mode() == game::mining::MiningMode::Micro) {
        auto& harvested = microHarvestCells_[outcome.block];
        harvested += outcome.microCellsRemoved;
        while (harvested >= static_cast<std::size_t>(world::micro::cellCount)) {
            spawnBlockDrop(outcome.block, hit);
            harvested -= static_cast<std::size_t>(world::micro::cellCount);
        }
    } else if (outcome.brokeBlock) {
        spawnBlockDrop(outcome.block, hit);
    }

    if (outcome.brokeBlock) {
        currentMiningTarget_.reset();
        currentMiningProgress_ = 0.0f;
    }
}

void VulkanRenderer::placeTargetBlock() {
    const auto selected = selectedPlacementBlock();
    if (!selected) return;

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

    if (world_.setBlock(hit.adjacent.x, hit.adjacent.y, hit.adjacent.z, *selected)) {
        (void)inventory_.removeFromSlot(inventory_.selectedHotbar(), 1);
        particles_.emitBlockBurst(*selected,
                                  {static_cast<float>(hit.adjacent.x) + 0.5f,
                                   static_cast<float>(hit.adjacent.y) + 0.5f,
                                   static_cast<float>(hit.adjacent.z) + 0.5f},
                                  4u, 0.45f);
        const auto& definition = world::blocks::BlockRegistry::get(*selected);
        audioEvents_.emitBlock(game::audio::AudioEventType::BlockPlace, definition.soundFamily,
                               {static_cast<float>(hit.adjacent.x) + 0.5f,
                                static_cast<float>(hit.adjacent.y) + 0.5f,
                                static_cast<float>(hit.adjacent.z) + 0.5f},
                               0.72f, true);
    }
}

void VulkanRenderer::updateWindowTitle() {
    if (!hwnd_) return;
    const auto position = player_.position();
    const auto stream = world_.streamingStats();
    const auto& selectedStack = inventory_.selectedStack();
    const std::string selectedName = selectedStack.empty() ? "Empty" : std::string(game::items::itemName(selectedStack.item));
    const std::string modeName(game::mining::miningModeName(mining_.mode()));
    std::wstring selected(selectedName.begin(), selectedName.end());
    std::wstring mode(modeName.begin(), modeName.end());
    std::wstring gpu(gpuName_.begin(), gpuName_.end());
    std::wstring title = L"RuneForge Realms ";
    for (const char* p = RF_VERSION_STRING; *p; ++p) title.push_back(static_cast<wchar_t>(*p));
    title += paused_ ? L" - PAUSED" : L" - Frontier Realms Survival";
    title += L" | Mine: " + mode;
    title += L" | Hotbar: " + selected + L" x" + std::to_wstring(selectedStack.count);
    title += L" | XYZ " + std::to_wstring(static_cast<int>(std::floor(position.x))) + L", " +
             std::to_wstring(static_cast<int>(std::floor(position.y))) + L", " +
             std::to_wstring(static_cast<int>(std::floor(position.z)));
    title += L" | Chunks " + std::to_wstring(stream.loaded) + L" + " + std::to_wstring(stream.pending) + L" pending";
    title += L" | Micro " + std::to_wstring(world_.promotedBlockCount());
    title += L" | Drops " + std::to_wstring(drops_.drops().size());
    title += L" | FX " + std::to_wstring(particles_.size());
    title += L" | " + gpu;
    if (paused_) title += L" | Esc: Resume";
    else title += L" | Hold LMB Swing/Mine | RMB Place | M Mining Mode | 1-9 Hotbar | Tab/I Inventory | Esc Pause";
    SetWindowTextW(hwnd_, title.c_str());
}

void VulkanRenderer::setError(std::wstring message) { lastError_ = std::move(message); }

void VulkanRenderer::setError(const wchar_t* prefix, VkResult result) {
    lastError_ = std::wstring(prefix) + L" (VkResult " + std::to_wstring(static_cast<int>(result)) + L")";
}

} // namespace rf::render

#endif