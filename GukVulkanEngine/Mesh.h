#pragma once

#include "Device.h"
#include "Buffer.h"
#include "DataStructures.h"

namespace guk {

class Mesh
{
  public:
    Mesh(std::shared_ptr<Device> device);

    void addVertex(Vertex vertex);
    void addIndex(uint32_t index);

    void createVertexBuffer();
    void createIndexBuffer();

    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;

    uint32_t indicesSize() const;

    void setMaterialIndex(uint32_t index);
    uint32_t getMaterialIndex() const;

    void calculateTangents();

  private:
    std::shared_ptr<Device> device_;

    std::vector<Vertex> vertices_{};
    std::vector<uint32_t> indices_{};

    std::shared_ptr<Buffer> vertexBuffer_;
    std::shared_ptr<Buffer> indexBuffer_;

    uint32_t materialIndex_{0};
};

} // namespace guk