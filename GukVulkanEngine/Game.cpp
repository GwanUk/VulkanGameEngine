#include "Game.h"
#include "Logger.h"
#include "Model.h"

#include <imgui.h>
#include <chrono>

namespace guk {

Game::Game()
    : window_(std::make_unique<Window>()),
      device_(std::make_shared<Device>(window_->getRequiredExts())),
      swapchain_(std::make_unique<Swapchain>(window_, device_)),
      renderer_(std::make_unique<Renderer>(device_, swapchain_->width(), swapchain_->height())),
      rendererPost_(std::make_unique<RendererPost>(
          device_, swapchain_->format(), swapchain_->width(), swapchain_->height(),
          renderer_->colorAttachment(), renderer_->shadowAttachment())),
      rendererGui_(std::make_unique<RendererGui>(device_, swapchain_->format()))
{
    setCallBack();
    createSyncObjects();
    createModels();
    renderer_->allocateModelDescriptorSets(models_);
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
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!window_->shouldClose()) {
        window_->pollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime =
            std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime)
                .count();
        lastTime = currentTime;
        deltaTime = glm::min(deltaTime, 0.033f); // Max 33ms (30 FPS minimum)

        calculatePerformanceMetrics(deltaTime);
        updateGui();

        camera_.update(deltaTime);
        camera_.writeScene(sceneUniform_);
        calculateDirectionalLight();

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
        auto app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));

        if (action == GLFW_PRESS) {
            switch (key) {
            case GLFW_KEY_W:
                app->camera_.keyState_.forward = true;
                break;
            case GLFW_KEY_S:
                app->camera_.keyState_.backward = true;
                break;
            case GLFW_KEY_A:
                app->camera_.keyState_.left = true;
                break;
            case GLFW_KEY_D:
                app->camera_.keyState_.right = true;
                break;
            case GLFW_KEY_E:
                app->camera_.keyState_.down = true;
                break;
            case GLFW_KEY_Q:
                app->camera_.keyState_.up = true;
                break;
            case GLFW_KEY_F:
                app->camera_.firstPersonMode_ = !app->camera_.firstPersonMode_;
                break;
            case GLFW_KEY_G:
                app->postUniform_.shadowDepthView = ~app->postUniform_.shadowDepthView;
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            }
        } else if (action == GLFW_RELEASE) {
            switch (key) {
            case GLFW_KEY_W:
                app->camera_.keyState_.forward = false;
                break;
            case GLFW_KEY_S:
                app->camera_.keyState_.backward = false;
                break;
            case GLFW_KEY_A:
                app->camera_.keyState_.left = false;
                break;
            case GLFW_KEY_D:
                app->camera_.keyState_.right = false;
                break;
            case GLFW_KEY_E:
                app->camera_.keyState_.down = false;
                break;
            case GLFW_KEY_Q:
                app->camera_.keyState_.up = false;
                break;
            }
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

        if (!ImGui::GetIO().WantCaptureMouse && app->mouseState_.buttons.left) {
            app->camera_.rotate(x - app->mouseState_.position.x, y - app->mouseState_.position.y);
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

void Game::createModels()
{
    models_.push_back(Model::load(device_, "assets\\DamagedHelmet\\glTF-"
                                           "Binary\\DamagedHelmet.glb")
                          .setRotation(glm::vec3(180.f, 0.f, 0.f)));

    models_.push_back(Model::load(device_, "assets\\Sponza\\glTF\\Sponza.gltf")
                          .setTranslation(glm::vec3(0.f, -1.f, 0.f))
                          .setRotation(glm::vec3(0.f, 90.f, 0.f)));
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

void Game::calculatePerformanceMetrics(float deltaTime)
{
    cpuTimesSinceLastUpdate_ += deltaTime;
    cpuFramesSinceLastUpdate_++;

    if (cpuTimesSinceLastUpdate_ >= 0.5f) {
        // CPU
        currentCpuFps_ = static_cast<float>(cpuFramesSinceLastUpdate_) / cpuTimesSinceLastUpdate_;
        currentCpuFps_ = glm::clamp(currentCpuFps_, 0.1f, 1e3f);
        cpuTimesSinceLastUpdate_ = 0.f;
        cpuFramesSinceLastUpdate_ = 0;

        // GPU
        currentGpuFps_ = static_cast<float>(gpuFramesSinceLastUpdate_) / gpuTimesSinceLastUpdate_;
        currentGpuFps_ = glm::clamp(currentGpuFps_, 0.1f, 1e3f);
        gpuTimesSinceLastUpdate_ = 0.f;
        gpuFramesSinceLastUpdate_ = 0;
    }
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

    if (ImGui::Begin("Render Settings")) {
        // Cpu/Gpu Performance Metrics
        if (ImGui::CollapsingHeader("CPU/GPU Performance Metrics",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            // CPU FPS
            ImVec4 cpuFpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            if (currentCpuFps_ < 30.0f) {
                cpuFpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            } else if (currentCpuFps_ < 60.0f) {
                cpuFpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            }

            ImGui::TextColored(cpuFpsColor, "!");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Performance Indicator\nCPU: Frame rate (includes CPU "
                                  "overhead)\nGreen: >60 FPS\nYellow: 30-60 FPS\nRed: <30 FPS");
            }
            ImGui::SameLine();
            ImGui::Text("CPU FPS: %.1f (%.2f ms/frame)", currentCpuFps_,
                        1e3f / std::max(currentCpuFps_, 1.0f));

            // GPU FPS
            ImVec4 gpuFpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            if (currentGpuFps_ < 30.0f) {
                gpuFpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            } else if (currentGpuFps_ < 60.0f) {
                gpuFpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            }

            ImGui::TextColored(gpuFpsColor, "!");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Performance Indicator\nGPU: Pure GPU rendering time (excludes "
                                  "presentation)\nGreen: >60 FPS\nYellow: 30-60 FPS\nRed: <30 FPS");
            }
            ImGui::SameLine();
            ImGui::Text("GPU FPS: %.1f (%.2f ms/frame)", currentGpuFps_,
                        1e3f / std::max(currentGpuFps_, 1.0f));
        }

        // Meshes Rendering Metrics
        if (ImGui::CollapsingHeader("Meshes Rendering Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Meshes Rendered: %d", renderer_->renderedMeshes_);
            ImGui::Text("Meshes Culled: %d", renderer_->culledMeshes_);
            ImGui::Text("Meshes Total: %d", renderer_->totalMeshes_);
        }

        // Camera Information
        if (ImGui::CollapsingHeader("Camera Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camera_.pos().x, camera_.pos().y,
                        camera_.pos().z);
            ImGui::Text("Camera Rotation: (%.2f°, %.2f°, %.2f°)", camera_.rot().x, camera_.rot().y,
                        camera_.rot().z);
            ImGui::Text("Camera Direction: (%.2f, %.2f, %.2f)", camera_.dir().x, camera_.dir().y,
                        camera_.dir().z);

            ImGui::Checkbox("First Person Mode", &camera_.firstPersonMode_);
        }

        // Directional Light Controls
        if (ImGui::CollapsingHeader("Directional Light Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Directional Light Dir: (%.2f, %.2f, %.2f)",
                        sceneUniform_.directionalLightDir.x, sceneUniform_.directionalLightDir.y,
                        sceneUniform_.directionalLightDir.z);

            static float elevation = 65.f;
            ImGui::SliderFloat("Light Elevation", &elevation, -90.0f, 90.0f, "%.1f°");
            static float azimuth = 10.f;
            ImGui::SliderFloat("Light Azimuth", &azimuth, -180.0f, 180.0f, "%.1f°");

            sceneUniform_.directionalLightDir =
                glm::vec3(cos(glm::radians(elevation)) * sin(glm::radians(azimuth)),
                          sin(glm::radians(elevation)),
                          cos(glm::radians(elevation)) * cos(glm::radians(azimuth)));

            static std::array<int, 3> color{255, 255, 255};
            ImGui::SliderInt3("Light Color", color.data(), 0, 255);
            static float lightIntensity = 1.0f;
            ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 100.0f, "%.2f");

            glm::vec3 lightColor = glm::vec3(color[0], color[1], color[2]) / 255.f;
            sceneUniform_.directionalLightColor = lightColor * lightIntensity;
        }

        // HDR Environment Controls
        if (ImGui::CollapsingHeader("HDR Environment Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Env Intensity", &skyboxUniform_.environmentIntensity, 0.0f, 10.0f,
                               "%.2f");
            ImGui::SliderFloat("Roughness Level", &skyboxUniform_.roughnessLevel, 0.0f, 10.0f,
                               "%.1f");

            bool useIrradiance = skyboxUniform_.useIrradianceMap != 0;
            if (ImGui::Checkbox("Use Irradiance Map", &useIrradiance)) {
                skyboxUniform_.useIrradianceMap = useIrradiance ? 1 : 0;
            }
        }

        // Post Processing Controls
        if (ImGui::CollapsingHeader("Post Processing Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Bloom Strength", &postUniform_.bloomStrength, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Exposure", &postUniform_.exposure, 0.1f, 5.0f, "%.2f");
            ImGui::SliderFloat("Gamma", &postUniform_.gamma, 1.0f / 2.2f, 2.2f, "%.2f");

            bool shadowDepthView = postUniform_.shadowDepthView != 0;
            if (ImGui::Checkbox("Shadow Depth View", &shadowDepthView)) {
                postUniform_.shadowDepthView = shadowDepthView ? 1 : 0;
            }

            ImGui::SliderFloat("Depth Scale", &postUniform_.depthScale, 0.0f, 1.0f, "%.2f");
        }

        // Models Controls
        if (ImGui::CollapsingHeader("Models Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (uint32_t i = 0; i < models_.size(); i++) {
                auto& m = models_[i];

                ImGui::Checkbox(std::format("{}##{}", m.name(), i).c_str(), &m.visible());

                std::array<float, 3> pos{m.getTranslation().x, m.getTranslation().y,
                                         m.getTranslation().z};
                if (ImGui::SliderFloat3(std::format("Position##{}", i).c_str(), pos.data(), -50.0f,
                                        50.0f, "%.2f")) {
                    m.setTranslation(glm::vec3(pos[0], pos[1], pos[2]));
                }

                std::array<float, 3> rot{m.getRotation().x, m.getRotation().y, m.getRotation().z};
                if (ImGui::SliderFloat3(std::format("Rotation##{}", i).c_str(), rot.data(), -180.0f,
                                        180.0f, "%.2f")) {
                    m.setRotation(glm::vec3(rot[0], rot[1], rot[2]));
                }

                std::array<float, 3> scale{m.getScale().x, m.getScale().y, m.getScale().z};
                if (ImGui::SliderFloat3(std::format("Scale##{}", i).c_str(), scale.data(), 0.1f,
                                        50.0f, "%.2f")) {
                    m.setScale(glm::vec3(scale[0], scale[1], scale[2]));
                }
            }
        }
    }

    ImGui::End();
    ImGui::Render();
}

void Game::calculateDirectionalLight()
{
    glm::vec3 forward = -sceneUniform_.directionalLightDir;
    glm::vec3 up = glm::vec3(0.f, 0.f, 1.f);
    if (glm::abs(glm::dot(forward, up)) > 0.99f) {
        up = glm::vec3(0.f, 1.f, 0.f);
    }
    glm::mat4 lightView = glm::lookAt(glm::vec3(0.f), forward, up);

    glm::vec3 wMin(std::numeric_limits<float>::max());
    glm::vec3 wMax(std::numeric_limits<float>::lowest());
    for (const auto& model : models_) {
        for (const auto& corner : ViewFrustum::corners(model.boundMin(), model.boundMax())) {
            glm::vec3 wCorner = glm::vec3(model.matrix() * glm::vec4(corner, 1.f));
            wMin = glm::min(wMin, wCorner);
            wMax = glm::max(wMax, wCorner);
        }
    }

    glm::vec3 vMin(std::numeric_limits<float>::max());
    glm::vec3 vMax(std::numeric_limits<float>::lowest());
    for (const auto& coner : ViewFrustum::corners(wMin, wMax)) {
        glm::vec3 vConer = glm::vec3(lightView * glm::vec4(coner, 1.f));
        vMin = glm::min(vMin, vConer);
        vMax = glm::max(vMax, vConer);
    }

    glm::mat4 lightProj = glm::orthoRH_ZO(vMin.x, vMax.x, vMin.y, vMax.y, -vMax.z, -vMin.z);
    lightProj[1][1] *= -1;

    sceneUniform_.directionalLightMatrix = lightProj * lightView;
    postUniform_.inverseProj = glm::inverse(lightProj);
}

void Game::drawFrame()
{
    VK_CHECK(vkWaitForFences(device_->get(), 1, &fences_[frameIdx_], VK_TRUE, UINT64_MAX));

    uint64_t timestamps[2];
    if (queryDataReady_[frameIdx_]) {
        VkResult result = vkGetQueryPoolResults(device_->get(), device_->queryPools(frameIdx_), 0,
                                                2, sizeof(timestamps), timestamps, sizeof(uint64_t),
                                                VK_QUERY_RESULT_64_BIT);
        if (result == VK_SUCCESS) {
            uint64_t timeDiff = timestamps[1] - timestamps[0];
            gpuTimesSinceLastUpdate_ +=
                static_cast<float>(timeDiff) * device_->timestampPeriod() * 1e-9f;
            gpuFramesSinceLastUpdate_++;
        }
    }
    queryDataReady_[frameIdx_] = true;

    uint32_t imageIdx{};
    VkResult result =
        vkAcquireNextImageKHR(device_->get(), swapchain_->get(), UINT64_MAX,
                              drawSemaphores_[semaphoreIdx_], VK_NULL_HANDLE, &imageIdx);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        exitLog("failed to acquire swap chain image!");
    }

    renderer_->update(frameIdx_, sceneUniform_, skyboxUniform_);
    rendererPost_->update(frameIdx_, postUniform_);
    rendererGui_->update(frameIdx_);

    VK_CHECK(vkResetFences(device_->get(), 1, &fences_[frameIdx_]));
    VK_CHECK(vkResetCommandBuffer(device_->cmdBuffers(frameIdx_), 0));

    VkCommandBuffer cmd = device_->cmdBuffers(frameIdx_);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    vkCmdResetQueryPool(cmd, device_->queryPools(frameIdx_), 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, device_->queryPools(frameIdx_), 0);

    renderer_->drawShadow(cmd, frameIdx_, models_);
    renderer_->draw(cmd, frameIdx_, models_);

    swapchain_->image(imageIdx)->transition(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    rendererPost_->draw(cmd, frameIdx_, swapchain_->image(imageIdx));
    rendererGui_->draw(cmd, frameIdx_, swapchain_->image(imageIdx));

    swapchain_->image(imageIdx)->transition(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                            VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, device_->queryPools(frameIdx_),
                        1);

    VK_CHECK(vkEndCommandBuffer(device_->cmdBuffers(frameIdx_)));

    VkSemaphoreSubmitInfo waitSemaphoreSI{};
    waitSemaphoreSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreSI.semaphore = drawSemaphores_[semaphoreIdx_];
    waitSemaphoreSI.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    waitSemaphoreSI.value = 0;
    waitSemaphoreSI.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreSI{};
    signalSemaphoreSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreSI.semaphore = prsntSemaphores_[semaphoreIdx_];
    signalSemaphoreSI.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreSI.value = 0;
    signalSemaphoreSI.deviceIndex = 0;

    VkCommandBufferSubmitInfo cmdBufferSI{};
    cmdBufferSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferSI.commandBuffer = device_->cmdBuffers(frameIdx_);
    cmdBufferSI.deviceMask = 0;

    VkSubmitInfo2 si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    si.waitSemaphoreInfoCount = 1;
    si.pWaitSemaphoreInfos = &waitSemaphoreSI;
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cmdBufferSI;
    si.signalSemaphoreInfoCount = 1;
    si.pSignalSemaphoreInfos = &signalSemaphoreSI;

    VK_CHECK(vkQueueSubmit2(device_->queue(), 1, &si, fences_[frameIdx_]));

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &prsntSemaphores_[semaphoreIdx_];
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

    frameIdx_ = (frameIdx_ + 1) % Device::MAX_FRAMES_IN_FLIGHT;
    semaphoreIdx_ = (semaphoreIdx_ + 1) % swapchain_->size();
}

} // namespace guk