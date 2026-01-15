#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

layout(set = 1, binding = 0) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform sampler2D brdfLUT;

layout(set = 2, binding = 0) uniform MaterialUniform {
    vec4 emissiveFactor;
    vec4 baseColorFactor;
    float roughnessFactor;
    float metallicFactor;
    int baseColorTextureIndex;
    int emissiveTextureIndex;
    int normalTextureIndex;
    int metallicRoughnessTextureIndex;
    int occlusionTextureIndex;
} material;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D emissiveTexture;
layout(set = 2, binding = 3) uniform sampler2D normalTexture;
layout(set = 2, binding = 4) uniform sampler2D metallicRoughnessTexture;
layout(set = 2, binding = 5) uniform sampler2D occlusionTexture;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 baseColorRGBA = texture(baseColorTexture, inTexcoord);
	vec3 baseColor = material.baseColorFactor.rgb * baseColorRGBA.rgb;

	vec3 lightVec = vec3(0.0, -1.0, 0.0);
//	color *= clamp(dot(lightVec, inNormal), 0.0, 1.0);

	outColor = vec4(baseColor, 1.0);
}