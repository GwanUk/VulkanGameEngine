#version 450

layout (location = 0) out vec2 outUV;

vec2 positions[6] = vec2[](
	vec2(-1.0, -1.0),  // Bottom-left
    vec2( 1.0, -1.0),  // Bottom-right  
    vec2(-1.0,  1.0),  // Top-left
    vec2(-1.0,  1.0),  // Top-left
    vec2( 1.0, -1.0),  // Bottom-right
    vec2( 1.0,  1.0)   // Top-right
);

vec2 UVs[6] = vec2[](
    vec2(0.0, 0.0),    // Bottom-left maps to tex(0,0) 
    vec2(1.0, 0.0),    // Bottom-right maps to tex(1,0)
    vec2(0.0, 1.0),    // Top-left maps to tex(0,1)
    vec2(0.0, 1.0),    // Top-left maps to tex(0,1)
    vec2(1.0, 0.0),    // Bottom-right maps to tex(1,0)
    vec2(1.0, 1.0)     // Top-right maps to tex(1,1)
);

void main()
{
    outUV = UVs[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}