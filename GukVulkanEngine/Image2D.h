#pragma once

#include "Engine.h"

namespace guk {
class Image2D
{
  public:
    Image2D(Engine& engine);
    ~Image2D();

    VkExtent2D extent_{};
    VkFormat format_{};

    VkImage image_{};
    VkImageView view_{};
    VkSampler sampler_{};

    void transition(VkCommandBuffer cmd, VkPipelineStageFlagBits2 srcStageMask,
                    VkAccessFlagBits2 srcAccessMask, VkPipelineStageFlagBits2 dstStageMask,
                    VkAccessFlagBits2 dstAccessMask, VkImageLayout oldLayout,
                    VkImageLayout newLayout, uint32_t baseMipLevel = 0,
                    uint32_t levelCount = 1) const;

    void init(VkExtent2D extent, VkFormat format, VkImage image);

    void createImage(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage,
                     VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, uint32_t mipLevels = 1);
    void createView();

    void createTexture(unsigned char* data, int width, int height, int channels, VkFormat format,
                       VkImageUsageFlags usage, bool useMipmap = false);
    void createTexture(const std::string& image, VkFormat format, VkImageUsageFlags usage,
                       bool useMipmap = false);

  private:
    Engine& engine_;
    VkDeviceMemory memory_{};

    bool imgOwner{true};

    void clean();

    void createSampler();
    void generateMipmap(VkCommandBuffer cmd, uint32_t mipLevels) const;

    VkImageAspectFlags aspect() const;
};
} // namespace guk
