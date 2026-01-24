#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;

layout(set = 0, binding = 0) uniform SceneUniform{
	mat4 model;
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
	vec3 directionalLightDir;  // sruface-to-light
	vec3 directionalLightColor; // radiance
	mat4 directionalLightMatrix;
} scene;

layout(set = 0, binding = 1) uniform SkyboxUniform {
    float environmentIntensity;
    float roughnessLevel;
    uint useIrradianceMap;
} skybox;

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

const float PI = 3.1415926535897932384626433832795;

vec3 fresnelSchlick(vec3 f0, float cosTheta) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float ndfGGX(float roughness, float NdotH) {
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (NdotH * NdotH) * (alphaSq - 1.0) + 1.0;

    return alphaSq / (PI * (denom * denom));
}

float geometrySchlickGGX(float roughness, float NdotL, float NdotV) {
    float k = ((roughness + 1.0) * (roughness + 1.0)) / 8.0;
    float g1l = NdotL / (NdotL * (1.0 - k) + k);
    float g1v = NdotV / (NdotV * (1.0 - k) + k);

    return g1l * g1v;
}

void main() {
    vec4 baseColorTex = material.baseColorTextureIndex >= 0 ? texture(baseColorTexture, inTexcoord) : vec4(1.0);
    vec4 emissiveTex = material.emissiveTextureIndex >= 0 ? texture(emissiveTexture, inTexcoord) : vec4(1.0);
    vec4 metallicRoughnessTex = material.metallicRoughnessTextureIndex >= 0 ? texture(metallicRoughnessTexture, inTexcoord) : vec4(1.0);
    vec4 occlusionTex = material.occlusionTextureIndex >= 0 ? texture(occlusionTexture, inTexcoord) : vec4(1.0);

	vec3 baseColor = baseColorTex.rgb * material.baseColorFactor.rgb;
	vec3 emissive = emissiveTex.rgb * material.emissiveFactor.rgb;
	float roughness = clamp(metallicRoughnessTex.g * material.roughnessFactor, 0.0, 1.0);
	float metallic = clamp(metallicRoughnessTex.b * material.metallicFactor, 0.0, 1.0);
	float occlusion = occlusionTex.r;

    vec3 V = normalize(scene.cameraPos - inPosition);
    vec3 L = normalize(scene.directionalLightDir);
    vec3 H = normalize(V + L);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent - (dot(inTangent, N) * N));
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    if(material.normalTextureIndex >= 0) {
	    vec3 normalTS  = texture(normalTexture, inTexcoord).rgb * 2.0 - 1.0;
        if(dot(normalTS, normalTS) > 1e-4){
            N = normalize(TBN * normalTS);
        }
    }

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(V, H), 0.0);

    vec3 f0_dielectric = vec3(0.04);
    vec3 f0 = mix(f0_dielectric, baseColor, metallic);

    // ambient lighting
    vec3 f = fresnelSchlick(f0, NdotV);
    vec3 kd = mix(1.0 - f, vec3(0.0), metallic);
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIbl = kd * baseColor * irradiance;

    vec2 lut = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 radiance = textureLod(prefilteredMap, reflect(-V, N), roughness * 10.0).rgb; // max mipmap level is assumed to be 10
    vec3 specualrIbl = (lut.r * f0 + lut.g) * radiance;

    float envIntensity = skybox.environmentIntensity;
    vec3 ambientLighting = (diffuseIbl + specualrIbl) * envIntensity * occlusion;

    // directional lighting
    vec3 F = fresnelSchlick(f0, HdotV);
    vec3 l_kd = mix(1.0 - F, vec3(0.0), metallic);
    vec3 l_diffuseBRDF = l_kd * (baseColor / PI);

    float D = ndfGGX(roughness, NdotH);
    float G = geometrySchlickGGX(roughness, NdotL, NdotV);
    vec3 l_specularBRDF = (F * D * G) / max(4.0 * NdotL * NdotV, 1e-5);

    vec3 l_radiance = scene.directionalLightColor;
    vec3 directionalLighting = (l_diffuseBRDF + l_specularBRDF) * l_radiance * NdotL;

    vec4 color = vec4(ambientLighting + directionalLighting + emissive, 1.0);
    outColor = clamp(color, 0.0, 1000.0);
}