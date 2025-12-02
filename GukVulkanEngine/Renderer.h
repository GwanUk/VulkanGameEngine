#pragma once

#include "Image2D.h"

#include <array>

namespace guk {

class Renderer
{
  public:
    Renderer(std::shared_ptr<Device> device);
    ~Renderer();

    void createAttachments();
    void update(uint32_t frameIdx);
    void draw(uint32_t frameIdx, const std::unique_ptr<Image2D>& renderTarget);

  private:
    std::shared_ptr<Device> device_;

    VkBuffer vertexBuffer_{};
    VkDeviceMemory vertexMemory_{};
    VkBuffer indexBuffer_{};
    VkDeviceMemory indexMemory_{};

    std::array<VkBuffer, Device::MAX_FRAMES_IN_FLIGHT> sceneBuffers_{};
    std::array<VkDeviceMemory, Device::MAX_FRAMES_IN_FLIGHT> sceneMemory_{};
    std::array<void*, Device::MAX_FRAMES_IN_FLIGHT> sceneMapped_{};

    std::unique_ptr<Image2D> colorAttahcment_;
    std::unique_ptr<Image2D> depthStencilAttahcment_;
    std::unique_ptr<Image2D> textureImage_;

    VkDescriptorSetLayout descriptorSetLayout_{};
    std::array<VkDescriptorSet, Device::MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

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
