#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inCol;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(texSampler, inUV);
}