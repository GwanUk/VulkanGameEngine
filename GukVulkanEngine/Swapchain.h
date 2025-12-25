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
    std::shared_ptr<Image2D> image(uint32_t index) const;
    uint32_t size() const;
    uint32_t width() const;
    uint32_t height() const;
    const VkFormat& format() const;

  private:
    std::shared_ptr<Device> device_;
    VkSurfaceKHR surface_{};
    VkSwapchainKHR swapchain_{};
    std::vector<std::shared_ptr<Image2D>> images_{};
};
} // namespace guk
