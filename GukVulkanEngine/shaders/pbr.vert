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
	vec3 directionalLightDir;
	vec3 directionalLightColor;
	mat4 directionalLightMatrix;
} scene;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexcoord;
layout(location = 3) out vec3 outTangent;

void main() {
	vec3 worldPosition = vec3(scene.model * vec4(inPosition, 1.0));
	mat3 normalMatrix = transpose(inverse(mat3(scene.model)));

	outPosition = worldPosition;
	outNormal = normalize(normalMatrix * inNormal);
	outTangent = normalize(normalMatrix * inTangent);
	outTexcoord = inTexcoord;
	gl_Position = scene.proj * scene.view * vec4(worldPosition, 1.0);
}