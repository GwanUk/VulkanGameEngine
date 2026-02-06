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

std::vector<Vertex>& Mesh::vertices()
{
    return vertices_;
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

void Mesh::calculateTangents()
{
    std::vector<glm::vec3> tangents(vertices_.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indicesSize(); i += 3) {
        const uint32_t& i0 = indices_[i];
        const uint32_t& i1 = indices_[i + 1];
        const uint32_t& i2 = indices_[i + 2];

        const Vertex& v0 = vertices_[i0];
        const Vertex& v1 = vertices_[i1];
        const Vertex& v2 = vertices_[i2];

        glm::vec3 e1 = v1.position - v0.position;
        glm::vec3 e2 = v2.position - v0.position;

        glm::vec2 d1 = v1.texcoord - v0.texcoord;
        glm::vec2 d2 = v2.texcoord - v0.texcoord;

        float f = 1.0f / (d1.x * d2.y - d2.x * d1.y);
        glm::vec3 tangent = f * (d2.y * e1 - d1.y * e2);

        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;
    }

    for (size_t i = 0; i < vertices_.size(); i++) {
        glm::vec3 N = vertices_[i].normal;
        glm::vec3 T = tangents[i];
        vertices_[i].tangent = glm::normalize(T - glm::dot(T, N) * N);
    }
}

void Mesh::calculateBound()
{
    boundMin_ = glm::vec3(std::numeric_limits<float>::max());
    boundMax_ = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& vertex : vertices_) {
        boundMin_ = glm::min(boundMin_, vertex.position);
        boundMax_ = glm::max(boundMax_, vertex.position);
    }
}

glm::vec3 Mesh::boundMin() const
{
    return boundMin_;
}

glm::vec3 Mesh::boundMax() const
{
    return boundMax_;
}

void Mesh::setBounds(glm::vec3 min, glm::vec3 max)
{
    boundMin_ = min;
    boundMax_ = max;
}

} // namespace guk