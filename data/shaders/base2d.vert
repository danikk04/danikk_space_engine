#include<uniforms.glslh>
#include<vert.glslh>
#include<2d.glslh>

void main()
{
	frag_uv = apply_uv(uv, vertex_uv);
    gl_Position = apply_pos(world, vertex_pos);
}
