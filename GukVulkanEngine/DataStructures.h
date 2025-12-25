#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace guk {

struct Vertex
{
  public:
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 col;

    static VkVertexInputBindingDescription getBindingDescrption();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct SceneUniform
{
  public:
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct alignas(16) SkyboxUniform
{
    float environmentIntensity = 1.0f;
    float roughnessLevel = 0.5f;
    uint32_t useIrradianceMap = 0;
    uint32_t showMipLevels = 0;
    uint32_t showCubeFaces = 0;
};

struct alignas(16) PostUniform
{
    float strength = 0.0f;
    float exposure = 1.0f;
    float gamma = 2.2f;
};

struct BloomPushConstants
{
    float dx;
    float dy;
    float lod;
};

struct GuiPushConstants
{
    glm::vec2 scale{1.f, 1.f};
    glm::vec2 translate{0.f, 0.f};
};

struct MouseState
{
    struct
    {
        bool left = false;
        bool right = false;
        bool middle = false;
    } buttons;
    glm::vec2 position{0.0f, 0.0f};
};

static const std::vector<Vertex> vertices{{{-0.5f, -0.5f, 0.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
                                          {{0.5f, -0.5f, 0.f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
                                          {{0.5f, 0.5f, 0.f}, {0.f, 1.f}, {0.f, 0.f, 1.f}},
                                          {{-0.5f, 0.5f, 0.f}, {1.f, 1.f}, {1.f, 1.f, 1.f}},
                                          {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
                                          {{0.5f, -0.5f, -0.5f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
                                          {{0.5f, 0.5f, -0.5f}, {0.f, 1.f}, {0.f, 0.f, 1.f}},
                                          {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f}, {1.f, 1.f, 1.f}}};

static const std::vector<uint16_t> indices{0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

} // namespace guk