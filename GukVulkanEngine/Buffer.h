#pragma once

#include "Device.h"

namespace guk {

class Buffer
{
  public:
    Buffer(std::shared_ptr<Device> device);
    ~Buffer();

    void createStagingBuffer(const void* data, VkDeviceSize size);
    void createUniformBuffer(VkDeviceSize size);
    void createLocalBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlagBits usage);

    const VkBuffer& get() const;

    template <typename T_DATA>
    void update(const T_DATA& data)
    {
        if (mappedMemory_) {
            memcpy(mappedMemory_, &data, sizeof(T_DATA));
        }
    }

  private:
    std::shared_ptr<Device> device_;

    VkBuffer buffer_{};
    VkDeviceMemory memory_{};
    void* mappedMemory_{};

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property);
};

} // namespace guk