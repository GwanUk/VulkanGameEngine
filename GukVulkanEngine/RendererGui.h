#pragma once

#include "Image2D.h"

#include <array>

namespace guk {

class RendererGui
{
  public:
    RendererGui(std::shared_ptr<Device> device, VkFormat colorFormat);
    ~RendererGui();

    void update(uint32_t frameIdx);
    void draw(VkCommandBuffer cmd, uint32_t frameIdx, const std::shared_ptr<Image2D> renderTarget);

  private:
    std::shared_ptr<Device> device_;

    float scale_{1.4f};

    std::unique_ptr<Image2D> fontTexture_;

    VkDescriptorSetLayout descriptorSetLayout_{};
    VkDescriptorSet descriptorSet_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};

    std::array<VkBuffer, Device::MAX_FRAMES_IN_FLIGHT> vertexBuffers_{};
    std::array<VkDeviceMemory, Device::MAX_FRAMES_IN_FLIGHT> vertexBufferMemorys_{};
    std::array<void*, Device::MAX_FRAMES_IN_FLIGHT> vertexMappeds_{};
    std::array<VkDeviceSize, Device::MAX_FRAMES_IN_FLIGHT> vertexAllocationSizes_{};

    std::array<VkBuffer, Device::MAX_FRAMES_IN_FLIGHT> indexBuffers_{};
    std::array<VkDeviceMemory, Device::MAX_FRAMES_IN_FLIGHT> indexBufferMemorys_{};
    std::array<void*, Device::MAX_FRAMES_IN_FLIGHT> indexMappeds_{};
    std::array<VkDeviceSize, Device::MAX_FRAMES_IN_FLIGHT> indexAllocationSizes_{};

    void init();
    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void createPipelineLayout();
    void createPipeline(VkFormat colorFormat);

    void createBuffer(uint32_t frameIdx, VkBufferUsageFlagBits usage, VkDeviceSize size);
};

} // namespace guk
