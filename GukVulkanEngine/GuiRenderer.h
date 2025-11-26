#pragma once

#include "Engine.h"
#include "Image2D.h"

namespace guk {

class GuiRenderer
{
  public:
    GuiRenderer(Engine& engine);
    ~GuiRenderer();

    void update(uint32_t frameIdx);
    void draw(VkCommandBuffer cmd, uint32_t frameIdx);

  private:
    Engine& engine_;
    float scale_{1.4f};

    Image2D fontTexture_;

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
