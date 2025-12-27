#pragma once

#include "Image2D.h"
#include "Buffer.h"
#include "DataStructures.h"

namespace guk {
class RendererPost
{
  public:
    RendererPost(std::shared_ptr<Device> device, VkFormat colorFormat, uint32_t width,
                 uint32_t height, std::shared_ptr<Image2D> sceneTexture);
    ~RendererPost();

    void resized(uint32_t width, uint32_t height);

    void updatePostUniform(uint32_t frameIdx, PostUniform postUniform);

    void draw(VkCommandBuffer cmd, uint32_t frameIdx, std::shared_ptr<Image2D> swapchainImg);

  private:
    std::shared_ptr<Device> device_;
    static constexpr uint32_t BLOOM_LEVELS{4};

    std::array<std::unique_ptr<Buffer>, Device::MAX_FRAMES_IN_FLIGHT> postUniformBuffers_;
    std::unique_ptr<Image2D> bloomImage_;
    std::array<std::unique_ptr<Image2D>, BLOOM_LEVELS> bloomTextures_;
    std::shared_ptr<Image2D> sceneTexture_;

    VkDescriptorSetLayout uniformSetLayout_{};
    VkDescriptorSetLayout samplerSetLayout_{};

    std::array<VkDescriptorSet, Device::MAX_FRAMES_IN_FLIGHT> uniformSets_{};
    std::array<VkDescriptorSet, BLOOM_LEVELS> bloomSamplerSets_{};
    VkDescriptorSet sceneSamplerSet_{};

    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};
    VkPipeline pipelineBloomDown_{};
    VkPipeline pipelineBloomUp_{};

    void bloomDown(VkCommandBuffer cmd);
    void bloomUp(VkCommandBuffer cmd);

    void createBloomImage(uint32_t width, uint32_t height);
    void createUniform();

    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void updateSampelrDescriptorSet();

    void createPipelineLayout();
    void createPipeline(VkFormat colorFormat);

    void createPipelineBloomDown();
    void createPipelineBloomUp();
};
} // namespace guk
