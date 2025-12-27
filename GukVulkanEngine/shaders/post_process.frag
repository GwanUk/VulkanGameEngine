#version 450

layout (location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform PostUniform {
    float strength;
    float exposure;
    float gamma;
} postUniform;

layout(set = 1, binding = 0) uniform sampler2D bloomTexture;
layout(set = 2, binding = 0) uniform sampler2D sceneTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color0 = texture(sceneTexture, inUV).rgb;
    vec3 color1 = texture(bloomTexture, inUV).rgb;

    vec3 color = mix(color0, color1, postUniform.strength);

    color = clamp(postUniform.exposure * color, 0.0, 1.0);
    color = pow(color, vec3(1.0 / postUniform.gamma));

    outColor = vec4(color, 1.0);
}