#pragma once

#include "Swapchain.h"
#include "DataStructures.h"
#include "Renderer.h"
#include "RendererPost.h"
#include "RendererGui.h"
#include "Camera.h"

namespace guk {

class Game
{
  public:
    Game();
    ~Game();
    void run();

  private:
    std::unique_ptr<Window> window_;
    std::shared_ptr<Device> device_;
    std::unique_ptr<Swapchain> swapchain_;

    std::array<VkFence, Device::MAX_FRAMES_IN_FLIGHT> fences_;
    std::vector<VkSemaphore> drawSemaphores_;
    std::vector<VkSemaphore> prsntSemaphores_;

    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<RendererPost> rendererPost_;
    std::unique_ptr<RendererGui> rendererGui_;

    bool resized_{};
    MouseState mouseState_{};
    Camera camera_{};
    std::vector<Model> models_{};

    SceneUniform sceneUniform_{};
    SkyboxUniform skyboxUniform_{};
    PostUniform postUniform_{};

    uint32_t frameIdx_{};
    uint32_t semaphoreIdx_{};
    std::array<bool, Device::MAX_FRAMES_IN_FLIGHT> queryDataReady_;

    float currentCpuFps_{};
    float cpuTimesSinceLastUpdate_{};
    uint32_t cpuFramesSinceLastUpdate_{};

    float currentGpuFps_{};
    float gpuTimesSinceLastUpdate_{};
    uint32_t gpuFramesSinceLastUpdate_{};

    void setCallBack();
    void createSyncObjects();
    void createModels();

    void recreateSwapChain();
    void calculatePerformanceMetrics(float deltaTime);

    void updateGui();
    void calculateDirectionalLight();

    void drawFrame();
};

} // namespace guk
