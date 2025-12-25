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


    void updatePost(uint32_t frameIdx, PostUniform postUniform);
    void draw(VkCommandBuffer cmd, uint32_t frameIdx, std::shared_ptr<Image2D> swapchainImg);

    void resized(uint32_t width, uint32_t height);

  private:
    std::shared_ptr<Device> device_;
    static constexpr uint32_t BLOOM_LEVEL{4};

    std::shared_ptr<Image2D> sceneTexture_;
    std::unique_ptr<Image2D> bloomTexture_;

    std::array<std::unique_ptr<Image2D>, BLOOM_LEVEL> bloomAttachemnts_;

    std::array<std::unique_ptr<Buffer>, Device::MAX_FRAMES_IN_FLIGHT> postUniformBuffers_;

    VkDescriptorSetLayout descriptorSetLayout_{};
    std::array<VkDescriptorSet, Device::MAX_FRAMES_IN_FLIGHT> descriptorSets_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};

    void createAttachments(uint32_t width, uint32_t height);
    void createUniform();

    void createDescriptorSetLayout();
    void allocateDescriptorSets();
    void updateDescriptorSets();
    void createPipelineLayout();
    void createPipeline(VkFormat colorFormat);
};
} // namespace guk
