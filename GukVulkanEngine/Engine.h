#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace guk {

class Engine
{
  public:
    Engine();
    ~Engine();

    void run();

  private:
    GLFWwindow* window_{};
    bool glfwFramebufferResized_{false};

    VkInstance instance_{};
    VkPhysicalDevice physicalDevice_{};
    VkPhysicalDeviceProperties physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures physicalDeviceFeatures_{};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties_{};

    uint32_t queueFaimlyIndex_{uint32_t(-1)};
    VkDevice device_{};

    VkQueue queue_{};
    VkCommandPool commandPool_{};
    std::vector<VkCommandBuffer> commandBuffers_;
    VkFormat depthStencilFormat_{};
    VkSampleCountFlagBits sampleCount_{};
    VkPipelineCache pipelineCache_{};

    VkSurfaceKHR surface_{};
    VkSwapchainKHR swapchain_{};
    VkExtent2D swapchainImageExtent_{};
    VkFormat swapchainImageFormat_{};
    uint32_t swapchainImageCount_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapChainImageViews_;

    const uint32_t MAX_FRAMES_IN_FLIGHT{2};
    std::vector<VkFence> inFlightFences_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    uint32_t currentFrame_{};
    uint32_t currentSemaphore_{};

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

    VkDescriptorPool descriptorPool_{};
    VkDescriptorSetLayout descriptorSetLayout_{};
    std::vector<VkDescriptorSet> descriptorSets_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};

    VkCommandBuffer beginCommand();
    void submitAndWait(VkCommandBuffer commandBuffer);

    uint32_t getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty);

    VkShaderModule createShaderModuleFromSpvfile(const std::string& spvFilename);

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
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createTextureImage();
    void createColorAttahcment();
    void createDepthStencilAttachment();

    void createDescriptorPool();
    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void createPipelineLayout();
    void createPipeline();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
};

} // namespace guk
