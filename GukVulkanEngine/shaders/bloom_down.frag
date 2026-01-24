#version 450

layout (location = 0) in vec2 inUV;

layout(push_constant) uniform BloomPushConstants
{
    float width;
    float height;
} bloom;

layout(set = 1, binding = 0) uniform sampler2D bloomTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    float x = inUV.x;
    float y = inUV.y;
    float dx = 1.0 / bloom.width;
    float dy = 1.0 / bloom.height;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(bloomTexture, vec2(x - 2 * dx, y - 2 * dy)).rgb;
    vec3 b = texture(bloomTexture, vec2(x,          y - 2 * dy)).rgb;
    vec3 c = texture(bloomTexture, vec2(x + 2 * dx, y - 2 * dy)).rgb;

    vec3 d = texture(bloomTexture, vec2(x - 2 * dx, y)).rgb;
    vec3 e = texture(bloomTexture, vec2(x,          y)).rgb;
    vec3 f = texture(bloomTexture, vec2(x + 2 * dx, y)).rgb;

    vec3 g = texture(bloomTexture, vec2(x - 2 * dx, y + 2 * dy)).rgb;
    vec3 h = texture(bloomTexture, vec2(x,          y + 2 * dy)).rgb;
    vec3 i = texture(bloomTexture, vec2(x + 2 * dx, y + 2 * dy)).rgb;

    vec3 j = texture(bloomTexture, vec2(x - dx, y - dy)).rgb;
    vec3 k = texture(bloomTexture, vec2(x + dx, y - dy)).rgb;
    vec3 l = texture(bloomTexture, vec2(x - dx, y + dy)).rgb;
    vec3 m = texture(bloomTexture, vec2(x + dx, y + dy)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    vec3 color = e * 0.125;
    color += (a + c + g + i) * 0.03125;
    color += (b + d + f + h) * 0.0625;
    color += (j + k + l + m) * 0.125;

    outColor = vec4(color, 1.0);
}