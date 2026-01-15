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

    bool resized_{false};
    MouseState mouseState_{};
    Camera camera_{};

    std::vector<Model> models_{};

    SceneUniform sceneUniform_{};
    SkyboxUniform skyboxUniform_{};
    PostUniform postUniform_{};

    void setCallBack();
    void createSyncObjects();
    void createModels();

    void recreateSwapChain();
    void updateGui();
    void renderHDRControlGui();
    void drawFrame();
};

} // namespace guk
