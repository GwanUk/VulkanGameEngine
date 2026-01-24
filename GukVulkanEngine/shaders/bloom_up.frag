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

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(bloomTexture, vec2(x - dx, y - dy)).rgb;
    vec3 b = texture(bloomTexture, vec2(x,      y - dy)).rgb;
    vec3 c = texture(bloomTexture, vec2(x + dx, y - dy)).rgb;

    vec3 d = texture(bloomTexture, vec2(x - dx, y)).rgb;
    vec3 e = texture(bloomTexture, vec2(x,      y)).rgb;
    vec3 f = texture(bloomTexture, vec2(x + dx, y)).rgb;
                                  
    vec3 g = texture(bloomTexture, vec2(x - dx, y + dy)).rgb;
    vec3 h = texture(bloomTexture, vec2(x,      y + dy)).rgb;
    vec3 i = texture(bloomTexture, vec2(x + dx, y + dy)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    vec3 color = e * 4.0;
    color += (b + d + f + h) * 2.0;
    color += (a + c + g + i);
    color *= 1.0 / 16.0;

    outColor = vec4(color, 1.0);
}