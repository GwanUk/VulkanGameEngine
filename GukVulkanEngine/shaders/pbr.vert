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

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexcoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec4 outLightSpacePos;

void main() {
	outPosition = vec3(modelPc.model * vec4(inPosition, 1.0));
	outNormal = normalize(transpose(inverse(mat3(modelPc.model))) * inNormal);
	outTangent = normalize(mat3(modelPc.model) * inTangent);
	outTexcoord = inTexcoord;

	const mat4 scaleBias = mat4(
        0.5, 0.0, 0.0, 0.0, 
        0.0, 0.5, 0.0, 0.0, 
        0.0, 0.0, 1.0, 0.0, 
        0.5, 0.5, 0.0, 1.0
	);
	outLightSpacePos = scaleBias * scene.directionalLightMatrix * vec4(outPosition, 1.0);

	gl_Position = scene.proj * scene.view * vec4(outPosition, 1.0);
}