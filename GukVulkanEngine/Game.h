#pragma once

#include "Swapchain.h"
#include "DataStructures.h"
#include "Renderer.h"
#include "RendererGui.h"

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
    std::unique_ptr<RendererGui> rendererGui_;

    MouseState mouseState_{};
    bool resized_{false};

    void setCallBack();
    void createSyncObjects();

    void recreateSwapChain();
    void updateGui();
    void drawFrame();
};

} // namespace guk
