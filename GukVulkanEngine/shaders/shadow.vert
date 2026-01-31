#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;

layout(push_constant) uniform ModelPushConstants
{
    mat4 model;
} modelPc;

layout(set = 0, binding = 0) uniform SceneUniform{
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
	vec3 directionalLightDir;
	vec3 directionalLightColor;
	mat4 directionalLightMatrix;
} scene;

void main() {
	gl_Position = scene.directionalLightMatrix * modelPc.model * vec4(inPosition, 1.0);
}