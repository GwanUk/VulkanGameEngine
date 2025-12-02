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

    VkInstance iHnd() const;
    VkPhysicalDevice pHnd() const;
    VkDevice hnd() const;

    VkQueue queue() const;
    VkPipelineCache cache() const;

    VkFormat depthStencilFormat() const;
    VkFormat textureFormat(bool srgb = true) const;
    const VkFormat& getColorFormat() const;
    void setColorFormat(VkFormat format);

    VkExtent2D getExtent() const;
    void setExtent(VkExtent2D extent);
    uint32_t width() const;
    uint32_t height() const;

    void checkSurfaceSupport(VkSurfaceKHR surface) const;
    VkSampleCountFlagBits smapleCount() const;
    uint32_t getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty) const;
    VkShaderModule createShaderModule(const std::string& spv) const;

    VkCommandBuffer cmdBuffers(uint32_t index) const;
    VkCommandBuffer beginCmd() const;
    void submitWait(VkCommandBuffer cmd) const;
    const VkDescriptorPool& descriptorPool() const;

  private:
    VkInstance instance_{};
    VkPhysicalDevice pDevice_{};
    VkDevice device_{};
    VkPipelineCache cache_{};

    uint32_t queueFaimlyIdx_{uint32_t(-1)};
    VkQueue queue_{};

    VkCommandPool cmdPool_{};
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuffers_{};
    VkDescriptorPool descPool_{};

    VkFormat dsFmt_{};
    std::array<VkFormat, 2> texFmts_{VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM};
    VkFormat colorFmt_{};
    VkExtent2D extent_{};

    const char* validationLayer_{"VK_LAYER_KHRONOS_validation"};
    const char* swapchainExtension_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    void createInstance(std::vector<const char*> instanceExtensions);
    void selectPhysicalDevice();
    void createDevice();
    void createPipelineCache();

    void createCommandPool();
    void createDescriptorPool();
};

} // namespace guk
