#include "Image2D.h"
#include "Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace guk {

Image2D::Image2D(std::shared_ptr<Device> device) : device_(device)
{
}

Image2D::~Image2D()
{
    clean();
}

VkImage Image2D::get() const
{
    return image_;
}

VkImageView Image2D::view() const
{
    return view_;
}

VkSampler Image2D::sampler() const
{
    return sampler_;
}

void Image2D::clean()
{
    if (sampler_) {
        vkDestroySampler(device_->hnd(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (view_) {
        vkDestroyImageView(device_->hnd(), view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }
    if (image_ && imgOwner_) {
        vkDestroyImage(device_->hnd(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (memory_) {
        vkFreeMemory(device_->hnd(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

void Image2D::transition(VkCommandBuffer cmd, VkPipelineStageFlagBits2 srcStageMask,
                         VkAccessFlagBits2 srcAccessMask, VkPipelineStageFlagBits2 dstStageMask,
                         VkAccessFlagBits2 dstAccessMask, VkImageLayout oldLayout,
                         VkImageLayout newLayout, uint32_t baseMipLevel, uint32_t levelCount) const
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStageMask;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange.aspectMask = aspect();
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &di);
}

void Image2D::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                          VkSampleCountFlagBits samples, uint32_t mipLevels)
{
    clean();
    format_ = format;

    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.extent.width = width;
    imageCI.extent.height = height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = mipLevels;
    imageCI.arrayLayers = 1;
    imageCI.format = format_;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage = usage;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.samples = samples;

    VK_CHECK(vkCreateImage(device_->hnd(), &imageCI, nullptr, &image_));

    VkMemoryRequirements memoryRs;
    vkGetImageMemoryRequirements(device_->hnd(), image_, &memoryRs);

    VkMemoryAllocateInfo memoryAI{};
    memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex =
        device_->getMemoryTypeIndex(memoryRs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_->hnd(), &memoryAI, nullptr, &memory_));
    VK_CHECK(vkBindImageMemory(device_->hnd(), image_, memory_, 0));

    createView();
}

void Image2D::createView()
{
    VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.image = image_;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = format_;
    imageViewCI.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                              VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    imageViewCI.subresourceRange.aspectMask = aspect();
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(device_->hnd(), &imageViewCI, nullptr, &view_));
}

void Image2D::createView(VkImage image, VkFormat format)
{
    clean();
    image_ = image;
    format_ = format;
    imgOwner_ = false;
    createView();
}

void Image2D::createTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels,
                            VkFormat format, VkImageUsageFlags usage, bool useMipmap)
{
    VkDeviceSize dataSize =
        static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * channels;

    uint32_t mipLevels =
        useMipmap ? static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1 : 1;

    VkBuffer stagingbuffer{};
    VkDeviceMemory stagingMemory{};

    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = dataSize;
    bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_->hnd(), &bufferCI, nullptr, &stagingbuffer));

    VkMemoryRequirements memoryRs;
    vkGetBufferMemoryRequirements(device_->hnd(), stagingbuffer, &memoryRs);

    VkMemoryAllocateInfo memoryAI{};
    memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex = device_->getMemoryTypeIndex(
        memoryRs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device_->hnd(), &memoryAI, nullptr, &stagingMemory));
    VK_CHECK(vkBindBufferMemory(device_->hnd(), stagingbuffer, stagingMemory, 0));

    void* mapped;
    VK_CHECK(vkMapMemory(device_->hnd(), stagingMemory, 0, dataSize, 0, &mapped));
    memcpy(mapped, data, static_cast<size_t>(dataSize));
    vkUnmapMemory(device_->hnd(), stagingMemory);
    stbi_image_free(data);

    createImage(width, height, format, usage, VK_SAMPLE_COUNT_1_BIT, mipLevels);
    createSamplerAnisoRepeat();

    VkCommandBuffer cmd = device_->beginCmd();

    transition(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
               VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(cmd, stagingbuffer, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);

    if (useMipmap) {
        generateMipmap(cmd, mipLevels, static_cast<int32_t>(width), static_cast<int32_t>(height));
    }

    transition(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               mipLevels - 1);

    device_->submitWait(cmd);

    vkDestroyBuffer(device_->hnd(), stagingbuffer, nullptr);
    vkFreeMemory(device_->hnd(), stagingMemory, nullptr);
}

void Image2D::createTexture(const std::string& image, VkFormat format, VkImageUsageFlags usage,
                            bool useMipmap)
{
    int width, height, ch;
    unsigned char* data = stbi_load(image.c_str(), &width, &height, &ch, STBI_rgb_alpha);
    if (!data) {
        exitLog("failed to load image: {}", image);
    }

    createTexture(data, width, height, 4, format, usage, useMipmap);
}

void Image2D::createSamplerAnisoRepeat()
{
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(device_->pHnd(), &features);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device_->pHnd(), &properties);

    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.mipLodBias = 0.f;
    samplerCI.anisotropyEnable = features.samplerAnisotropy;
    samplerCI.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerCI.compareEnable = VK_FALSE;
    samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCI.minLod = 0.f;
    samplerCI.maxLod = VK_LOD_CLAMP_NONE;
    samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCI.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(device_->hnd(), &samplerCI, nullptr, &sampler_));
}

void Image2D::createSamplerAnisoClamp()
{
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(device_->pHnd(), &features);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device_->pHnd(), &properties);

    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.mipLodBias = 0.f;
    samplerCI.anisotropyEnable = features.samplerAnisotropy;
    samplerCI.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerCI.compareEnable = VK_FALSE;
    samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCI.minLod = 0.f;
    samplerCI.maxLod = VK_LOD_CLAMP_NONE;
    samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCI.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(device_->hnd(), &samplerCI, nullptr, &sampler_));
}

void Image2D::createSamplerLinearRepeat()
{
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.mipLodBias = 0.f;
    samplerCI.anisotropyEnable = VK_FALSE;
    samplerCI.maxAnisotropy = 1.f;
    samplerCI.compareEnable = VK_FALSE;
    samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCI.minLod = 0.f;
    samplerCI.maxLod = VK_LOD_CLAMP_NONE;
    samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCI.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(device_->hnd(), &samplerCI, nullptr, &sampler_));
}

void Image2D::createSamplerLinearClamp()
{
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.mipLodBias = 0.f;
    samplerCI.anisotropyEnable = VK_FALSE;
    samplerCI.maxAnisotropy = 1.f;
    samplerCI.compareEnable = VK_FALSE;
    samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCI.minLod = 0.f;
    samplerCI.maxLod = VK_LOD_CLAMP_NONE;
    samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCI.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(device_->hnd(), &samplerCI, nullptr, &sampler_));
}

void Image2D::generateMipmap(VkCommandBuffer cmd, uint32_t mipLevels, int32_t width,
                             int32_t height) const
{
    for (uint32_t i = 1; i < mipLevels; i++) {
        transition(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   i - 1);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {width, height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        transition(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   i - 1);

        if (width > 1) {
            width /= 2;
        }
        if (height > 1) {
            height /= 2;
        }
    }
}

VkImageAspectFlags Image2D::aspect() const
{
    VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};

    switch (format_) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    case VK_FORMAT_S8_UINT:
        aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    }

    return aspect;
}

} // namespace guk