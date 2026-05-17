#version 460

layout (location = 0) uniform vec2 position;
layout (location = 1) uniform vec2 scale;

void main()
{
	vec2 verts[] = {
		vec2(-1, 1),
		vec2(-1, -1),
		vec2(1, 1),
		vec2(1, 1),
		vec2(-1, -1),
		vec2(1, -1),
	};

	gl_Position = vec4((verts[gl_VertexID] * scale) + position, 0, 1);
}