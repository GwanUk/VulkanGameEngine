#pragma once

#include "DataStructures.h"
#include "Logger.h"

#include <GLFW/glfw3.h>
#include <array>

namespace guk {

class SceneRenderer;
class GuiRenderer;

class Engine
{
  public:
    Engine();
    ~Engine();

    MouseState mouseState_{};

    GuiRenderer* guiRenderer_;
    SceneRenderer* sceneRenderer_;

    VkPhysicalDeviceProperties physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures physicalDeviceFeatures_{};

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT{2};

    VkDevice device_{};
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers_;

    VkSampleCountFlagBits sampleCount_{};
    VkPipelineCache pipelineCache_{};

    VkFormat depthStencilFormat_{};
    VkFormat swapchainImageFormat_{};
    VkExtent2D swapchainImageExtent_{};

    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapChainImageViews_;

    VkDescriptorPool descriptorPool_{};

    uint32_t currentFrame_{};
    uint32_t currentSemaphore_{};
    uint32_t currentImage_{};

    void run();

    VkCommandBuffer beginCommand() const;
    void submitAndWait(VkCommandBuffer cmd) const;

    uint32_t getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty) const;

    VkShaderModule createShaderModule(const std::string& spv) const;

  private:
    VkInstance instance_{};
    VkPhysicalDevice physicalDevice_{};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties_{};

    uint32_t queueFaimlyIndex_{uint32_t(-1)};
    VkQueue queue_{};
    VkCommandPool commandPool_{};

    bool framebufferResized_{false};
    GLFWwindow* window_{};
    VkSurfaceKHR surface_{};
    VkSwapchainKHR swapchain_{};
    uint32_t swapchainImageCount_{};

    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;

    void createWindow();
    void createInstance();
    void selectPhysicalDevice();
    void createDevice();

    void createCommandPool();
    void allocateCommandBuffers();
    void determineDepthStencilFormat();
    void selectSmapleCount();
    void createPipelineCache();

    void createSwapChain();
    void recreateSwapChain();
    void createDescriptorPool();
    void createSyncObjects();

    void updateGui();
    void drawFrame();
};

} // namespace guk
