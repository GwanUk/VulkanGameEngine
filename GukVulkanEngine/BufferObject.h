#pragma once

#include "BufferObject.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace guk {

class Vertex
{
  public:
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescrption();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class SceneUniform
{
  public:
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

static const std::vector<Vertex> vertices{{{-0.5f, -0.5f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
                                   {{0.5f, -0.5f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
                                   {{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
                                   {{-0.5f, 0.5f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
                                   {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
                                   {{0.5f, -0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
                                   {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
                                   {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 1.f}, {1.f, 1.f}}};

static const std::vector<uint16_t> indices{0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

} // namespace guk