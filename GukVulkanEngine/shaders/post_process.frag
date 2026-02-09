#version 450

layout (location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform PostUniform {
    mat4 inverseProj;
    uint shadowDepthView;
    float depthScale;
    float bloomStrength;
    float exposure;
    float gamma;
} post;

layout(set = 1, binding = 0) uniform sampler2D bloomTexture;
layout(set = 2, binding = 0) uniform sampler2D sceneTexture;
layout(set = 3, binding = 0) uniform sampler2D shadowTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color;

    if(post.shadowDepthView == 0){
        vec3 color0 = texture(sceneTexture, inUV).rgb;
        vec3 color1 = texture(bloomTexture, inUV).rgb;

        color = mix(color0, color1, post.bloomStrength);
        color *= post.exposure;
        color = color / (color + vec3(1.0)); // Reinhard Tone Mapping
        color = pow(color, vec3(1.0 / post.gamma));
    } else {
        vec4 ndcPosition;
        ndcPosition.xy = inUV * 2.0 - 1.0;
        ndcPosition.z = texture(shadowTexture, inUV).r;
        ndcPosition.w = 1.0;

        vec4 viewPosition = post.inverseProj * ndcPosition;
        viewPosition /= viewPosition.w;
        color = vec3(-viewPosition.z * post.depthScale);
    }

    outColor = vec4(color, 1.0);
}