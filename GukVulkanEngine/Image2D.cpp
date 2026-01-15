#include "Image2D.h"
#include "Logger.h"
#include "Buffer.h"

#include <ktx.h>
#include <ktxvulkan.h>
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

void Image2D::setSampler(VkSampler sampler)
{
    sampler_ = sampler;
}

VkSampler Image2D::sampler() const
{
    return sampler_;
}

uint32_t Image2D::width() const
{
    return width_;
}

uint32_t Image2D::height() const
{
    return height_;
}

const VkFormat& Image2D::format() const
{
    return format_;
}

void Image2D::createImage(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
                          VkSampleCountFlagBits samples, uint32_t baseMipLevel, uint32_t mipLevels)
{
    format_ = format;
    width_ = width;
    height_ = height;
    baseMipLevel_ = baseMipLevel;
    mipLevels_ = mipLevels;
    createImage(usage, samples, 0, VK_IMAGE_VIEW_TYPE_2D);
}

void Image2D::createView(VkImage image, VkFormat format, uint32_t width, uint32_t height,
                         uint32_t baseMipLevel, uint32_t mipLevels)
{
    clean();
    imgOwner_ = false;

    image_ = image;
    format_ = format;
    width_ = width;
    height_ = height;
    baseMipLevel_ = baseMipLevel;
    mipLevels_ = mipLevels;
    createView(VK_IMAGE_VIEW_TYPE_2D);
}

void Image2D::createTexture(const unsigned char* data, uint32_t width, uint32_t height,
                            uint32_t channels, bool srgb)
{
    format_ = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    width_ = width;
    height_ = height;
    mipLevels_ = 1;
    arrayLayers_ = 1;

    VkDeviceSize size =
        static_cast<VkDeviceSize>(width_) * static_cast<VkDeviceSize>(height_) * channels;

    Buffer buffer{device_};
    buffer.createStagingBuffer(data, size);

    createImage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT,
                0, VK_IMAGE_VIEW_TYPE_2D);

    VkCommandBuffer cmd = device_->beginCmd();

    transition(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {width_, height_, 1};

    vkCmdCopyBufferToImage(cmd, buffer.get(), image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);

    transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    device_->submitWait(cmd);
}

void Image2D::createTexture(const std::string& image, bool srgb)
{
    int width, height, ch;

    unsigned char* data = stbi_load(image.c_str(), &width, &height, &ch, STBI_rgb_alpha);

    if (!data) {
        exitLog("failed to load image: {}", image);
    }

    createTexture(data, width, height, 4, srgb);

    stbi_image_free(data);
}

void Image2D::createTextureFromMemory(const unsigned char* data, int size, bool srgb)
{
    int width, height, ch;

    unsigned char* newData =
        stbi_load_from_memory(data, size, &width, &height, &ch, STBI_rgb_alpha);

    if (!newData) {
        exitLog("failed to load data from memory");
    }

    createTexture(newData, width, height, 4, srgb);

    stbi_image_free(newData);
}

void Image2D::createTextureKtx2(const std::string& image, bool isSkybox)
{
    ktxTexture2* texture2;
    if (ktxTexture2_CreateFromNamedFile(image.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                        &texture2) != KTX_SUCCESS) {
        exitLog("Failed to load KTX2 texture: {}", image);
    }

    format_ = ktxTexture2_GetVkFormat(texture2);
    if (format_ == VK_FORMAT_UNDEFINED) {
        format_ = isSkybox ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R16G16_SFLOAT;
    }
    width_ = texture2->baseWidth;
    height_ = texture2->baseHeight;
    mipLevels_ = texture2->numLayers;
    arrayLayers_ = isSkybox ? 6 : 1;

    ktxTexture* texture = ktxTexture(texture2);
    ktx_uint8_t* textureData = ktxTexture_GetData(texture);
    ktx_size_t textureSize = ktxTexture_GetDataSize(texture);

    Buffer buffer{device_};
    buffer.createStagingBuffer(textureData, textureSize);

    VkImageCreateFlags flags = isSkybox ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    VkImageViewType viewType = isSkybox ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;

    createImage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT,
                flags, viewType);

    VkCommandBuffer cmd = device_->beginCmd();

    std::vector<VkBufferImageCopy> copies;

    for (uint32_t layer = 0; layer < arrayLayers_; layer++) {
        for (uint32_t level = 0; level < mipLevels_; level++) {
            ktx_size_t offset{};
            if (ktxTexture_GetImageOffset(texture, level, 0, layer, &offset) != KTX_SUCCESS) {
                offset = 0;
            }

            VkBufferImageCopy copy{};
            copy.bufferOffset = offset;
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel = level;
            copy.imageSubresource.baseArrayLayer = layer;
            copy.imageSubresource.layerCount = 1;
            copy.imageExtent.width = std::max(1u, width_ >> level);
            copy.imageExtent.height = std::max(1u, height_ >> level);
            copy.imageExtent.depth = 1;

            copies.push_back(copy);
        }
    }

    transition(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyBufferToImage(cmd, buffer.get(), image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(copies.size()), copies.data());

    transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    device_->submitWait(cmd);

    ktxTexture_Destroy(ktxTexture(texture2));
}

void Image2D::transition(VkCommandBuffer cmd, VkPipelineStageFlagBits2 stage,
                         VkAccessFlagBits2 access, VkImageLayout layout)
{
    VkImageMemoryBarrier2 barrier = barrier2(stage, access, layout);

    VkDependencyInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &di);
}

VkImageMemoryBarrier2 Image2D::barrier2(VkPipelineStageFlagBits2 stage, VkAccessFlagBits2 access,
                                        VkImageLayout layout)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = currentStage_;
    barrier.srcAccessMask = currentAccess_;
    barrier.dstStageMask = stage;
    barrier.dstAccessMask = access;
    barrier.oldLayout = currentLayout_;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange.aspectMask = aspect();
    barrier.subresourceRange.baseMipLevel = baseMipLevel_;
    barrier.subresourceRange.levelCount = mipLevels_;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = arrayLayers_;

    currentStage_ = stage;
    currentAccess_ = access;
    currentLayout_ = layout;

    return barrier;
}

void Image2D::clean()
{
    if (sampler_) {
        sampler_ = VK_NULL_HANDLE;
    }
    if (view_) {
        vkDestroyImageView(device_->get(), view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }
    if (image_ && imgOwner_) {
        vkDestroyImage(device_->get(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (memory_) {
        vkFreeMemory(device_->get(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }

    currentStage_ = VK_PIPELINE_STAGE_2_NONE;
    currentAccess_ = VK_ACCESS_2_NONE;
    currentLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
}

void Image2D::createImage(VkImageUsageFlags usage, VkSampleCountFlagBits samples,
                          VkImageCreateFlags flags, VkImageViewType viewType)
{
    clean();

    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.extent.width = width_;
    imageCI.extent.height = height_;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = mipLevels_;
    imageCI.arrayLayers = arrayLayers_;
    imageCI.format = format_;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage = usage;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.samples = samples;
    imageCI.flags = flags;

    VK_CHECK(vkCreateImage(device_->get(), &imageCI, nullptr, &image_));

    VkMemoryRequirements memoryRs;
    vkGetImageMemoryRequirements(device_->get(), image_, &memoryRs);

    VkMemoryAllocateInfo memoryAI{};
    memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex =
        device_->getMemoryTypeIndex(memoryRs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_->get(), &memoryAI, nullptr, &memory_));
    VK_CHECK(vkBindImageMemory(device_->get(), image_, memory_, 0));

    createView(viewType);
}

void Image2D::createView(VkImageViewType viewType)
{
    VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.image = image_;
    imageViewCI.viewType = viewType;
    imageViewCI.format = format_;
    imageViewCI.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                              VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    imageViewCI.subresourceRange.aspectMask = aspect();
    imageViewCI.subresourceRange.baseMipLevel = baseMipLevel_;
    imageViewCI.subresourceRange.levelCount = mipLevels_;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = arrayLayers_;

    VK_CHECK(vkCreateImageView(device_->get(), &imageViewCI, nullptr, &view_));
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