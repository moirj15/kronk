#version 460

layout(binding = 0, std430) readonly buffer ModelData0 {
	float vertices[];
};
layout(binding = 1, std430) readonly buffer ModelData0 {
	uint indices[];
};

uniform mat4 mvp;


void main()
{
	uint index = indices[gl_vertexID];
	vec4 vertex = vec4(vertices[index * 3], vertices[index * 3 + 1], vertices[index * 3 + 2], 1.0);
	gl_Position = mvp * vertex;
}