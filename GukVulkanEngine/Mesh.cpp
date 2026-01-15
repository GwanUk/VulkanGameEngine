#include "Mesh.h"

namespace guk {

Mesh::Mesh(std::shared_ptr<Device> device)
    : device_(device), vertexBuffer_(std::make_shared<Buffer>(device_)),
      indexBuffer_(std::make_shared<Buffer>(device_))
{
}

void Mesh::addVertex(Vertex vertex)
{
    vertices_.push_back(vertex);
}

void Mesh::addIndex(uint32_t index)
{
    indices_.push_back(index);
}

void Mesh::createVertexBuffer()
{
    vertexBuffer_->createLocalBuffer(vertices_.data(), sizeof(vertices_[0]) * vertices_.size(),
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void Mesh::createIndexBuffer()
{
    indexBuffer_->createLocalBuffer(indices_.data(), sizeof(indices_[0]) * indices_.size(),
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

VkBuffer Mesh::getVertexBuffer() const
{
    return vertexBuffer_->get();
}

VkBuffer Mesh::getIndexBuffer() const
{
    return indexBuffer_->get();
}

uint32_t Mesh::indicesSize() const
{
    return static_cast<uint32_t>(indices_.size());
}

void Mesh::setMaterialIndex(uint32_t index)
{
    materialIndex_ = index;
}

uint32_t Mesh::getMaterialIndex() const
{
    return materialIndex_;
}

} // namespace guk