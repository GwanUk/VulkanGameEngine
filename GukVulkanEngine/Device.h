#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <array>
#include <string>

namespace guk {

class Device
{
  public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT{2};

    Device(const std::vector<const char*>& instanceExtensions);
    ~Device();

    VkInstance instance() const;
    VkPhysicalDevice physical() const;
    VkDevice get() const;

    VkQueue queue() const;
    VkPipelineCache cache() const;
    void checkSurfaceSupport(VkSurfaceKHR surface) const;

    VkFormat depthStencilFormat() const;
    VkSampleCountFlagBits smapleCount() const;
    uint32_t getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty) const;

    VkCommandBuffer cmdBuffers(uint32_t index) const;
    VkCommandBuffer beginCmd() const;
    void submitWait(VkCommandBuffer cmd) const;

    VkShaderModule createShaderModule(const std::string& spv) const;
    const VkDescriptorPool& descriptorPool() const;

    VkSampler samplerAnisoRepeat() const;
    VkSampler samplerAnisoClamp() const;
    VkSampler samplerLinearRepeat() const;
    VkSampler samplerLinearClamp() const;

  private:
    VkInstance instance_{};
    VkPhysicalDevice physicalDevice_{};
    VkDevice device_{};

    VkPipelineCache cache_{};
    VkFormat depthStencilFmt_{};

    uint32_t queueFaimlyIdx_{uint32_t(-1)};
    VkQueue queue_{};

    VkCommandPool cmdPool_{};
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuffers_{};
    VkDescriptorPool descPool_{};

    std::array<VkSampler, 4> samplers_{};

    const char* validationLayer_{"VK_LAYER_KHRONOS_validation"};
    const char* swapchainExtension_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    void createInstance(std::vector<const char*> instanceExtensions);
    void selectPhysicalDevice();
    void createDevice();
    void createPipelineCache();

    void createCommandPool();
    void createDescriptorPool();
    void createSamplers();
};

} // namespace guk
