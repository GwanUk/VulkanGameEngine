#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace guk {

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec3 tangent;

    static VkVertexInputBindingDescription getBindingDescrption();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct SceneUniform
{
    glm::mat4 view = glm::mat4(1.f);
    glm::mat4 proj = glm::mat4(1.f);
    glm::vec3 cameraPos = glm::vec3(0.f);
    alignas(16) glm::vec3 directionalLightDir = glm::vec3(0.f, 1.f, 0.f);
    alignas(16) glm::vec3 directionalLightColor = glm::vec3(1.f);
    alignas(16) glm::mat4 directionalLightMatrix = glm::mat4(1.f);
};

struct MaterialUniform
{
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float roughness = 1.0f;
    float metallicFactor = 0.0f;
    int32_t baseColorTextureIndex = -1;
    int32_t emissiveTextureIndex = -1;
    int32_t normalTextureIndex = -1;
    int32_t metallicRoughnessTextureIndex = -1;
    int32_t occlusionTextureIndex = -1;
};

struct alignas(16) SkyboxUniform
{
    float environmentIntensity = 1.0f;
    float roughnessLevel = 0.5f;
    uint32_t useIrradianceMap = 0;
};

struct PostUniform
{
    glm::mat4 inverseProj = glm::mat4(1.f);
    uint32_t shadowDepthView = 0;
    float depthScale = 0.1f;
    float bloomStrength = 0.1f;
    float exposure = 1.0f;
    float gamma = 2.2f;
};

struct ModelPushConstants
{
    glm::mat4 model = glm::mat4(1.f);
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

struct KeyState
{
    bool left{false};
    bool right{false};
    bool forward{false};
    bool backward{false};
    bool up{false};
    bool down{false};
};

} // namespace guk