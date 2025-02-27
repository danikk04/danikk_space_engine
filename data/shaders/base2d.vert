#include<vertex.glslh>
#include<uniforms.glslh>
#include<uv.glslh>
#include<pos.glslh>

void main()
{
	frag_uv = apply_uv(uv, vertex_uv);
    gl_Position = apply_pos(world, vertex_pos);
}