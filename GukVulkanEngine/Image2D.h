#pragma once

#include "Device.h"

#include <string>

namespace guk {

class Image2D
{
  public:
    Image2D(std::shared_ptr<Device> device);
    ~Image2D();

    VkImage get() const;
    VkImageView view() const;
    void setSampler(VkSampler sampler);
    VkSampler sampler() const;

    uint32_t width() const;
    uint32_t height() const;
    const VkFormat& format() const;

    void createImage(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
                     VkSampleCountFlagBits samples, uint32_t baseMipLevel = 0,
                     uint32_t mipLevels = 1);

    void createView(VkImage image, VkFormat format, uint32_t width, uint32_t height,
                    uint32_t baseMipLevel = 0, uint32_t mipLevels = 1);

    void createTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels,
                       bool srgb = false);
    void createTexture(const std::string& image, bool srgb = false);

    void createTextureKtx2(const std::string& image, bool isSkybox = false);

    void transition(VkCommandBuffer cmd, VkPipelineStageFlagBits2 stage, VkAccessFlagBits2 access,
                    VkImageLayout layout);
    VkImageMemoryBarrier2 barrier2(VkPipelineStageFlagBits2 stage, VkAccessFlagBits2 access,
                                   VkImageLayout layout);

  private:
    std::shared_ptr<Device> device_;
    bool imgOwner_{true};

    VkImage image_{};
    VkImageView view_{};
    VkDeviceMemory memory_{};
    VkSampler sampler_{};

    VkFormat format_{};
    uint32_t width_{};
    uint32_t height_{};

    uint32_t baseMipLevel_{0};
    uint32_t mipLevels_{1};
    uint32_t arrayLayers_{1};

    VkPipelineStageFlags2 currentStage_{};
    VkAccessFlags2 currentAccess_{};
    VkImageLayout currentLayout_{};

    void clean();

    void createImage(VkImageUsageFlags usage, VkSampleCountFlagBits samples,
                     VkImageCreateFlags flags, VkImageViewType viewType);
    void createView(VkImageViewType viewType);

    VkImageAspectFlags aspect() const;
};

} // namespace guk
