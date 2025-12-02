#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inCol;

layout(binding = 0) uniform SceneUniform{
	mat4 model;
	mat4 view;
	mat4 proj;
} scene;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outCol;

void main() {
	gl_Position = scene.proj * scene.view* scene.model * vec4(inPos, 1.0);
	outCol = inCol;
	outUV = inUV;
}