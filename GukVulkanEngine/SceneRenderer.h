#pragma once

#include "Engine.h"
#include "Image2D.h"

namespace guk {

class SceneRenderer
{
  public:
    SceneRenderer(Engine& engine);
    ~SceneRenderer();

    void createAttachments();
    void updateUniform(uint32_t frameIdx);
    void draw(VkCommandBuffer cmd, uint32_t frameIdx, Image2D& renderTarget);

  private:
    Engine& engine_;

    VkBuffer vertexBuffer_{};
    VkDeviceMemory vertexMemory_{};
    VkBuffer indexBuffer_{};
    VkDeviceMemory indexMemory_{};

    std::array<VkBuffer, Engine::MAX_FRAMES_IN_FLIGHT> sceneBuffers_{};
    std::array<VkDeviceMemory, Engine::MAX_FRAMES_IN_FLIGHT> sceneMemory_{};
    std::array<void*, Engine::MAX_FRAMES_IN_FLIGHT> sceneMapped_{};

    Image2D colorAttahcment_;
    Image2D depthStencilAttahcment_;
    Image2D textureImage_;

    VkDescriptorSetLayout descriptorSetLayout_{};
    std::array<VkDescriptorSet, Engine::MAX_FRAMES_IN_FLIGHT> descriptorSets_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};

    void createBuffers();
    void createUniformBuffers();
    void createTextures();

    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void createPipelineLayout();
    void createPipeline();
};

} // namespace guk
