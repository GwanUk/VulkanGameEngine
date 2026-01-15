#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace guk {

struct Vertex
{
  public:
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;

    static VkVertexInputBindingDescription getBindingDescrption();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct SceneUniform
{
  public:
    glm::mat4 model = glm::mat4(1.f);
    glm::mat4 view = glm::mat4(1.f);
    glm::mat4 proj = glm::mat4(1.f);
};

struct MaterialUniform
{
    glm::vec4 emissiveFactor_ = glm::vec4(0.0f);
    glm::vec4 baseColorFactor_ = glm::vec4(1.0f);
    float roughness_ = 1.0f;
    float metallicFactor_ = 0.0f;
    int32_t baseColorTextureIndex_ = -1;
    int32_t emissiveTextureIndex_ = -1;
    int32_t normalTextureIndex_ = -1;
    int32_t metallicRoughnessTextureIndex_ = -1;
    int32_t occlusionTextureIndex_ = -1;
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
    float strength = 0.12f;
    float exposure = 1.0f;
    float gamma = 2.2f;
};

struct BloomPushConstants
{
    float width;
    float height;
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

} // namespace guk