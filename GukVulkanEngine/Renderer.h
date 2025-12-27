#pragma once

#include "Image2D.h"
#include "Buffer.h"
#include "DataStructures.h"

namespace guk {

class Renderer
{
  public:
    Renderer(std::shared_ptr<Device> device, uint32_t width, uint32_t height);
    ~Renderer();

    void createAttachments(uint32_t width, uint32_t height);

    void updateScene(uint32_t frameIdx, SceneUniform sceneUniform);
    void updateSkybox(uint32_t frameIdx, SkyboxUniform skyboxUniform);

    void draw(VkCommandBuffer cmd, uint32_t frameIdx);

    std::shared_ptr<Image2D> colorAttachment() const;

  private:
    std::shared_ptr<Device> device_;

    std::unique_ptr<Image2D> msaaColorAttachment_;
    std::shared_ptr<Image2D> colorAttachment_;
    std::unique_ptr<Image2D> msaaDepthStencilAttachment_;
    std::unique_ptr<Image2D> depthStencilAttachment_;

    std::array<std::unique_ptr<Buffer>, Device::MAX_FRAMES_IN_FLIGHT> sceneUniformBuffers_;
    std::array<std::unique_ptr<Buffer>, Device::MAX_FRAMES_IN_FLIGHT> skyboxUniformBuffers_;
    std::array<std::unique_ptr<Image2D>, 3> skyboxTextures_;

    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts_{};
    std::array<VkDescriptorSet, Device::MAX_FRAMES_IN_FLIGHT> uniformDescriptorSets_{};
    VkDescriptorSet samplerDescriptorSet_{};

    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipelineSkybox_{};

    void createUniform();
    void createTextures();

    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void createPipelineLayout();

    void createPipelineSkybox();
};

} // namespace guk