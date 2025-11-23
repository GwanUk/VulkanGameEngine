#pragma once

#include "DataStructures.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace guk {

class GuiRenderer;

class Engine
{
  public:
    Engine();
    ~Engine();

    void run();

    uint32_t getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty) const;

    void createBuffer(VkBuffer& buffer, VkBufferUsageFlags bufferUsageFlags,
                      VkDeviceMemory& bufferMemory, VkMemoryPropertyFlags memoryPropertyFlags,
                      VkDeviceSize dataSize);

    void createTexture(VkImage& textureImage, VkDeviceMemory& textureImageMemory,
                       VkImageView& textureImageView, VkSampler& textureSampler,
                       unsigned char* pixelData, int width, int height, int channels, bool srgb,
                       bool mipmap);

    VkShaderModule createShaderModule(const std::string& spv) const;

    MouseState mouseState_{};

    VkDevice device_{};
    VkPipelineCache pipelineCache_{};
    VkQueue queue_{};

    VkDescriptorPool descriptorPool_{};

    VkFormat swapchainImageFormat_{};
    VkExtent2D swapchainImageExtent_{};
    std::vector<VkImageView> swapChainImageViews_;

    std::vector<VkCommandBuffer> commandBuffers_;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT{2};
    uint32_t currentFrame_{};
    uint32_t currentSemaphore_{};
    uint32_t currentImage_{};

  private:
    GLFWwindow* window_{};
    bool glfwFramebufferResized_{false};

    VkInstance instance_{};
    VkPhysicalDevice physicalDevice_{};
    VkPhysicalDeviceProperties physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures physicalDeviceFeatures_{};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties_{};

    uint32_t queueFaimlyIndex_{uint32_t(-1)};

    VkCommandPool commandPool_{};
    VkFormat depthStencilFormat_{};
    VkSampleCountFlagBits sampleCount_{};

    VkSurfaceKHR surface_{};
    VkSwapchainKHR swapchain_{};
    uint32_t swapchainImageCount_{};
    std::vector<VkImage> swapchainImages_;

    std::vector<VkFence> inFlightFences_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;

    GuiRenderer* guiRenderer_;

    VkBuffer vertexBuffer_{};
    VkDeviceMemory vertexBufferMemory_{};
    VkBuffer indexBuffer_{};
    VkDeviceMemory indexBufferMemory_{};

    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;

    VkImage textureImage_{};
    VkDeviceMemory textureImageMemory_{};
    VkImageView textureImageView_{};
    VkSampler textureSampler_{};

    VkImage colorAttahcment_{};
    VkDeviceMemory colorAttahcmentMemory_{};
    VkImageView colorAttahcmentView_{};

    VkImage depthStencilAttahcment_{};
    VkDeviceMemory depthStencilAttahcmentMemory_{};
    VkImageView depthStencilAttahcmentView_{};

    VkDescriptorSetLayout descriptorSetLayout_{};
    std::vector<VkDescriptorSet> descriptorSets_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};

    VkCommandBuffer beginCommand() const;
    void submitAndWait(VkCommandBuffer commandBuffer) const;

    void createBuffer(VkBuffer& buffer, VkBufferUsageFlags bufferUsageFlags,
                      VkDeviceMemory& bufferMemory, VkDeviceSize dataSize, const void* data);

    void createAttachment(VkImage& image, VkFormat format, VkImageUsageFlags imageUsageFlags,
                          VkDeviceMemory& imageMemory, VkMemoryPropertyFlags memoryPropertyFlags,
                          VkImageView& imageView, VkImageAspectFlags imageAspectFlags);

    void createTexture(VkImage& textureImage, VkDeviceMemory& textureImageMemory,
                       VkImageView& textureImageView, VkSampler& textureSampler,
                       const std::string& image, bool srgb, bool mipmap);

    void recreateSwapChain();
    void updateUniformBuffer(uint32_t currentImage);

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
    void createSyncObjects();
    void createResources();

    void createDescriptorPool();
    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void createPipelineLayout();
    void createPipeline();

    void recordCommandBuffer();
    void drawFrame();
};

} // namespace guk
