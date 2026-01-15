#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

layout(set = 0, binding = 0) uniform SceneUniform{
	mat4 model;
	mat4 view;
	mat4 proj;
} scene;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexcoord;

void main() {
	gl_Position = scene.proj * scene.view * scene.model * vec4(inPosition, 1.0);
	outNormal = transpose(inverse(mat3(scene.model))) * inNormal;
	outTexcoord = inTexcoord;
}