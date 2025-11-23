#pragma once

#include "Engine.h"
#include <array>

namespace guk {

class GuiRenderer
{
  public:
    GuiRenderer(Engine& engine);
    ~GuiRenderer();

    void update();
    void draw();

  private:
    Engine& engine_;
    float scale_{1.4f};

    VkImage textureImage_{};
    VkDeviceMemory textureMemory_{};
    VkImageView textureView_{};
    VkSampler textureSampler_{};

    VkDescriptorSetLayout descriptorSetLayout_{};
    VkDescriptorSet descriptorSet_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};

    std::array<VkBuffer, Engine::MAX_FRAMES_IN_FLIGHT> vertexBuffers_{};
    std::array<VkDeviceMemory, Engine::MAX_FRAMES_IN_FLIGHT> vertexBufferMemorys_{};
    std::array<void*, Engine::MAX_FRAMES_IN_FLIGHT> vertexMappeds_{};
    std::array<VkDeviceSize, Engine::MAX_FRAMES_IN_FLIGHT> vertexAllocationSizes_{};

    std::array<VkBuffer, Engine::MAX_FRAMES_IN_FLIGHT> indexBuffers_{};
    std::array<VkDeviceMemory, Engine::MAX_FRAMES_IN_FLIGHT> indexBufferMemorys_{};
    std::array<void*, Engine::MAX_FRAMES_IN_FLIGHT> indexMappeds_{};
    std::array<VkDeviceSize, Engine::MAX_FRAMES_IN_FLIGHT> indexAllocationSizes_{};

    void init();
    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void createPipelineLayout();
    void createPipeline();

    void createBuffer(VkBufferUsageFlagBits usage, VkDeviceSize size);
};

} // namespace guk
