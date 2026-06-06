#version 460 core


struct Vertex 
{
	float pos[3];	
};

layout(binding = 0, std430) readonly buffer VertexData {
	Vertex vertices[];
};
layout(binding = 1, std430) readonly buffer IndexData {
	uint indices[];
};

layout(location = 0) uniform mat4 mvp;

vec3 GetPosition(uint index)
{
	Vertex v = vertices[index];
	return vec3(v.pos[0], v.pos[1], v.pos[2]);
}


void main()
{
	uint index = indices[gl_VertexID];
	vec4 vertex = vec4(GetPosition(index), 1.0);
	gl_Position = mvp * vertex;
}