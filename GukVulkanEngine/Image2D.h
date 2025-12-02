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
    VkSampler sampler() const;

    void transition(VkCommandBuffer cmd, VkPipelineStageFlagBits2 srcStageMask,
                    VkAccessFlagBits2 srcAccessMask, VkPipelineStageFlagBits2 dstStageMask,
                    VkAccessFlagBits2 dstAccessMask, VkImageLayout oldLayout,
                    VkImageLayout newLayout, uint32_t baseMipLevel = 0,
                    uint32_t levelCount = 1) const;

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                     VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, uint32_t mipLevels = 1);

    void createView(VkImage image, VkFormat format);

    void createTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels,
                       VkFormat format, VkImageUsageFlags usage, bool useMipmap = false);
    void createTexture(const std::string& image, VkFormat format, VkImageUsageFlags usage,
                       bool useMipmap = false);

    void createSamplerAnisoRepeat();
    void createSamplerAnisoClamp();
    void createSamplerLinearRepeat();
    void createSamplerLinearClamp();

  private:
    std::shared_ptr<Device> device_;

    VkImage image_{};
    VkImageView view_{};
    VkDeviceMemory memory_{};
    VkSampler sampler_{};

    VkFormat format_{};
    bool imgOwner_{true};

    void clean();
    void createView();

    void generateMipmap(VkCommandBuffer cmd, uint32_t mipLevels, int32_t width,
                        int32_t height) const;
    VkImageAspectFlags aspect() const;
};
} // namespace guk
