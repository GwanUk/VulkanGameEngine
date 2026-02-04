#pragma once

#include "Image2D.h"
#include "Buffer.h"
#include "DataStructures.h"
#include "Model.h"
#include "ViewFrustum.h"

namespace guk {

class Renderer
{
  public:
    Renderer(std::shared_ptr<Device> device, uint32_t width, uint32_t height);
    ~Renderer();

    void allocateModelDescriptorSets(std::vector<Model>& models);
    void createAttachments(uint32_t width, uint32_t height);
    std::shared_ptr<Image2D> colorAttachment() const;
    std::shared_ptr<Image2D> shadowAttachment() const;

    void update(uint32_t frameIdx, SceneUniform sceneUniform, SkyboxUniform skyboxUniform);
    void draw(VkCommandBuffer cmd, uint32_t frameIdx, std::vector<Model> models);
    void drawShadow(VkCommandBuffer cmd, uint32_t frameIdx, std::vector<Model> models);

    uint32_t totalMeshes_{};
    uint32_t renderedMeshes_{};
    uint32_t culledMeshes_{};

  private:
    std::shared_ptr<Device> device_;
    ViewFrustum viewFrustum_{};

    std::unique_ptr<Image2D> msaaColorAttachment_;
    std::shared_ptr<Image2D> colorAttachment_;
    std::unique_ptr<Image2D> msaaDepthStencilAttachment_;
    std::unique_ptr<Image2D> depthStencilAttachment_;
    std::array<std::unique_ptr<Image2D>, 3> skyboxTextures_;
    std::shared_ptr<Image2D> dummyTexture_{};
    std::shared_ptr<Image2D> shadowAttachment_;
    VkSampler shadowSampler_{};

    std::array<std::unique_ptr<Buffer>, Device::MAX_FRAMES_IN_FLIGHT> sceneUniformBuffers_;
    std::array<std::unique_ptr<Buffer>, Device::MAX_FRAMES_IN_FLIGHT> skyboxUniformBuffers_;

    std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts_{};
    std::array<VkDescriptorSet, Device::MAX_FRAMES_IN_FLIGHT> uniformDescriptorSets_{};
    VkDescriptorSet mapDescriptorSet_{};

    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};
    VkPipeline pipelineSkybox_{};
    VkPipeline pipelineShadow_{};

    void createUniform();
    void createTextures();
    void createShadowMap();

    void createDescriptorSetLayout();
    void allocateDescriptorSets();

    void createPipelineLayout();
    void createPipeline();
    void createPipelineSkybox();
    void createPipelineShadow();
};

} // namespace guk