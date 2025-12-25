#include "Buffer.h"
#include "Logger.h"

namespace guk {

Buffer::Buffer(std::shared_ptr<Device> device) : device_(device)
{
}

Buffer::~Buffer()
{
    if (mappedMemory_) {
        vkUnmapMemory(device_->get(), memory_);
        mappedMemory_ = nullptr;
    }

    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_->get(), buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }

    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_->get(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

void Buffer::createStagingBuffer(void* data, VkDeviceSize size)
{
    createMappedBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(mappedMemory_, data, static_cast<size_t>(size));
    vkUnmapMemory(device_->get(), memory_);
    mappedMemory_ = nullptr;
}

void Buffer::createUniformBuffers(uint32_t size)
{
    createMappedBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

VkBuffer Buffer::get()
{
    return buffer_;
}

void Buffer::createMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = size;
    bufferCI.usage = usage;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_->get(), &bufferCI, nullptr, &buffer_));

    VkMemoryRequirements memoryRs;
    vkGetBufferMemoryRequirements(device_->get(), buffer_, &memoryRs);

    VkMemoryAllocateInfo memoryAI{};
    memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex = device_->getMemoryTypeIndex(
        memoryRs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device_->get(), &memoryAI, nullptr, &memory_));
    VK_CHECK(vkBindBufferMemory(device_->get(), buffer_, memory_, 0));
    VK_CHECK(vkMapMemory(device_->get(), memory_, 0, bufferCI.size, 0, &mappedMemory_));
}

} // namespace guk