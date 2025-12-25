#include "Game.h"
#include "Logger.h"

#include <imgui.h>

namespace guk {

Game::Game()
    : window_(std::make_unique<Window>()),
      device_(std::make_shared<Device>(window_->getRequiredExts())),
      swapchain_(std::make_unique<Swapchain>(window_, device_)),
      renderer_(std::make_unique<Renderer>(device_, swapchain_->width(), swapchain_->height())),
      rendererPost_(std::make_unique<RendererPost>(device_, swapchain_->format(),
                                                   swapchain_->width(), swapchain_->height(),
                                                   renderer_->colorAttachment())),
      rendererGui_(std::make_unique<RendererGui>(device_, swapchain_->format()))
{
    setCallBack();
    createSyncObjects();
}

Game::~Game()
{
    for (size_t i = 0; i < swapchain_->size(); i++) {
        vkDestroySemaphore(device_->get(), drawSemaphores_[i], nullptr);
        vkDestroySemaphore(device_->get(), prsntSemaphores_[i], nullptr);
    }

    for (const auto& fence : fences_) {
        vkDestroyFence(device_->get(), fence, nullptr);
    }
}

void Game::run()
{
    while (!window_->shouldClose()) {
        window_->pollEvents();
        updateGui();
        drawFrame();
    }

    VK_CHECK(vkDeviceWaitIdle(device_->get()));
}

void Game::setCallBack()
{
    window_->setUserPointer(this);
    window_->setFramebufferSizeCallback([](GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));
        app->resized_ = true;
    });
    window_->setKeyCallback([](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    });
    window_->setMouseButtonCallback([](GLFWwindow* window, int button, int action, int mods) {
        auto app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (action == GLFW_PRESS) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                app->mouseState_.buttons.left = true;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                app->mouseState_.buttons.right = true;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                app->mouseState_.buttons.middle = true;
                break;
            }
        } else if (action == GLFW_RELEASE) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                app->mouseState_.buttons.left = false;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                app->mouseState_.buttons.right = false;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                app->mouseState_.buttons.middle = false;
                break;
            }
        }
    });
    window_->setCursorPosCallback([](GLFWwindow* window, double xpos, double ypos) {
        auto app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));

        float x(static_cast<float>(xpos));
        float y(static_cast<float>(ypos));

        float dx = app->mouseState_.position.x - x;
        float dy = app->mouseState_.position.y - y;

        if (app->mouseState_.buttons.left) {
            app->camera_.rotate(dx, dy);
        }

        app->mouseState_.position = glm::vec2(x, y);
    });
}

void Game::createSyncObjects()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& fence : fences_) {
        VK_CHECK(vkCreateFence(device_->get(), &fenceInfo, nullptr, &fence));
    }

    drawSemaphores_.resize(swapchain_->size());
    prsntSemaphores_.resize(swapchain_->size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < swapchain_->size(); i++) {
        VK_CHECK(vkCreateSemaphore(device_->get(), &semaphoreInfo, nullptr, &drawSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(device_->get(), &semaphoreInfo, nullptr, &prsntSemaphores_[i]));
    }
}

void Game::recreateSwapChain()
{
    while (window_->isMinimized()) {
        window_->waitEvents();
    }

    VK_CHECK(vkDeviceWaitIdle(device_->get()));

    swapchain_->create(window_);

    renderer_->createAttachments(swapchain_->width(), swapchain_->height());
    rendererPost_->resized(swapchain_->width(), swapchain_->height());
}

void Game::updateGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(swapchain_->image(0)->width()),
                            static_cast<float>(swapchain_->image(0)->height()));
    io.MousePos = ImVec2(mouseState_.position.x, mouseState_.position.y);
    io.MouseDown[0] = mouseState_.buttons.left;
    io.MouseDown[1] = mouseState_.buttons.right;
    io.MouseDown[2] = mouseState_.buttons.middle;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Control")) {
        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("Mouse: Look around");
        ImGui::Text("WASD: Move");
        ImGui::Text("QE: Up/Down");
        ImGui::Text("F2: Toggle camera mode");
    }
    ImGui::End();

    renderHDRControlGui();

    ImGui::Render();
}

void Game::renderHDRControlGui()
{
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("HDR Controls")) {
        ImGui::End();
        return;
    }

    // HDR Environment Controls
    if (ImGui::CollapsingHeader("HDR Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Environment Intensity", &skyboxUniform_.environmentIntensity, 0.0f,
                           10.0f, "%.2f");
    }

    // Environment Map Controls
    if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Roughness Level", &skyboxUniform_.roughnessLevel, 0.0f, 8.0f, "%.1f");

        bool useIrradiance = skyboxUniform_.useIrradianceMap != 0;
        if (ImGui::Checkbox("Use Irradiance Map", &useIrradiance)) {
            skyboxUniform_.useIrradianceMap = useIrradiance ? 1 : 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("?")) {
            // Optional: Add click action here if needed
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle between prefiltered environment map (sharp reflections) and "
                              "irradiance map (diffuse lighting)");
        }
    }

    // Tone Mapping Controls
    if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Bloom Strength", &postUniform_.strength, 0.0f, 1.0f, "%.2f");
    }

    // Tone Mapping Controls
    if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Exposure", &postUniform_.exposure, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Gamma", &postUniform_.gamma, 1.0f / 2.2f, 2.2f, "%.2f");
    }

    // Debug Visualization
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        bool showMipLevels = skyboxUniform_.showMipLevels != 0;
        if (ImGui::Checkbox("Show Mip Levels", &showMipLevels)) {
            skyboxUniform_.showMipLevels = showMipLevels ? 1 : 0;
        }

        bool showCubeFaces = skyboxUniform_.showCubeFaces != 0;
        if (ImGui::Checkbox("Show Cube Faces", &showCubeFaces)) {
            skyboxUniform_.showCubeFaces = showCubeFaces ? 1 : 0;
        }
    }

    ImGui::End();
}

void Game::drawFrame()
{
    static uint32_t frameIdx = 0;
    static uint32_t semaphoreIdx = 0;
    uint32_t imageIdx = 0;

    VK_CHECK(vkWaitForFences(device_->get(), 1, &fences_[frameIdx], VK_TRUE, UINT64_MAX));

    VkResult result =
        vkAcquireNextImageKHR(device_->get(), swapchain_->get(), UINT64_MAX,
                              drawSemaphores_[semaphoreIdx], VK_NULL_HANDLE, &imageIdx);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        exitLog("failed to acquire swap chain image!");
    }

    camera_.updateScene(sceneUniform_);

    renderer_->updateScene(frameIdx, sceneUniform_);
    renderer_->updateSkybox(frameIdx, skyboxUniform_);
    rendererPost_->updatePost(frameIdx, postUniform_);
    rendererGui_->update(frameIdx);

    VK_CHECK(vkResetFences(device_->get(), 1, &fences_[frameIdx]));
    VK_CHECK(vkResetCommandBuffer(device_->cmdBuffers(frameIdx), 0));

    VkCommandBuffer cmd = device_->cmdBuffers(frameIdx);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    swapchain_->image(imageIdx)->transition(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    renderer_->draw(cmd, frameIdx);
    rendererPost_->draw(cmd, frameIdx, swapchain_->image(imageIdx));
    rendererGui_->draw(cmd, frameIdx, swapchain_->image(imageIdx));

    swapchain_->image(imageIdx)->transition(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                            VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(device_->cmdBuffers(frameIdx)));

    VkSemaphoreSubmitInfo waitSemaphoreSI{};
    waitSemaphoreSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreSI.semaphore = drawSemaphores_[semaphoreIdx];
    waitSemaphoreSI.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    waitSemaphoreSI.value = 0;
    waitSemaphoreSI.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreSI{};
    signalSemaphoreSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreSI.semaphore = prsntSemaphores_[semaphoreIdx];
    signalSemaphoreSI.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreSI.value = 0;
    signalSemaphoreSI.deviceIndex = 0;

    VkCommandBufferSubmitInfo cmdBufferSI{};
    cmdBufferSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferSI.commandBuffer = device_->cmdBuffers(frameIdx);
    cmdBufferSI.deviceMask = 0;

    VkSubmitInfo2 si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    si.waitSemaphoreInfoCount = 1;
    si.pWaitSemaphoreInfos = &waitSemaphoreSI;
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cmdBufferSI;
    si.signalSemaphoreInfoCount = 1;
    si.pSignalSemaphoreInfos = &signalSemaphoreSI;

    VK_CHECK(vkQueueSubmit2(device_->queue(), 1, &si, fences_[frameIdx]));

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &prsntSemaphores_[semaphoreIdx];
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_->get();
    pi.pImageIndices = &imageIdx;
    pi.pResults = nullptr;

    result = vkQueuePresentKHR(device_->queue(), &pi);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized_) {
        resized_ = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        exitLog("failed to present swap chain image!");
    }

    frameIdx = (frameIdx + 1) % Device::MAX_FRAMES_IN_FLIGHT;
    semaphoreIdx = (semaphoreIdx + 1) % swapchain_->size();
}

} // namespace guk