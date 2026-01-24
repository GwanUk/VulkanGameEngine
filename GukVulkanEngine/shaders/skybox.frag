#version 450

layout(location = 0) in vec3 inPos;

layout(set = 0, binding = 1) uniform SkyboxUniform {
    float environmentIntensity;
    float roughnessLevel;
    uint useIrradianceMap;
} skybox;

layout(set = 1, binding = 0) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform sampler2D brdfLUT;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 envColor;
    
    if (skybox.useIrradianceMap != 0) {
        envColor = texture(irradianceMap, inPos).rgb;
    } else {
        envColor = textureLod(prefilteredMap, inPos, skybox.roughnessLevel).rgb;
    }
    
    envColor *= skybox.environmentIntensity;

    outColor = vec4(envColor, 1.0);
}