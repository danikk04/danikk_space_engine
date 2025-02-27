#include<vert_3d.glsl>

void main()
{
	frag_uv = vertex_uv;
    gl_Position = world * vec4(vertex_pos, 1.0);
}