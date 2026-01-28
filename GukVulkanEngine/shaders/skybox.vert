#version 450

layout(set = 0, binding = 0) uniform SceneUniform{
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
	vec3 directionalLightDir;
	vec3 directionalLightColor;
	mat4 directionalLightMatrix;
} scene;

layout(location = 0) out vec3 outPos;

const vec3 positions[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0),
	vec3( 1.0,-1.0, 1.0),
	vec3( 1.0, 1.0, 1.0),
	vec3(-1.0, 1.0, 1.0),

	vec3(-1.0,-1.0,-1.0),
	vec3( 1.0,-1.0,-1.0),
	vec3( 1.0, 1.0,-1.0),
	vec3(-1.0, 1.0,-1.0)
);

const int indices[36] = int[36](
	0, 1, 2, 2, 3, 0,	// front
	1, 5, 6, 6, 2, 1,	// right 
	7, 6, 5, 5, 4, 7,	// back
	4, 0, 3, 3, 7, 4,	// left
	4, 5, 1, 1, 0, 4,	// bottom
	3, 2, 6, 6, 7, 3	// top
);

void main() {
    vec3 pos = positions[indices[gl_VertexIndex]];
    vec4 clipPos = scene.proj * mat4(mat3(scene.view)) * vec4(pos, 1.0);
    
    outPos = pos;
    gl_Position = clipPos.xyww;
}