#pragma once

#include "Window.h"
#include "Device.h"
#include "Image2D.h"

namespace guk {

class Swapchain
{
  public:
    Swapchain(std::unique_ptr<Window>& window, std::shared_ptr<Device> device);
    ~Swapchain();
    void create(std::unique_ptr<Window>& window);

    const VkSwapchainKHR& get() const;
    const std::unique_ptr<Image2D>& image(uint32_t index) const;
    size_t size() const;

  private:
    std::shared_ptr<Device> device_;
    VkSurfaceKHR surface_{};
    VkSwapchainKHR swapchain_{};
    std::vector<std::unique_ptr<Image2D>> images_{};
};
} // namespace guk
