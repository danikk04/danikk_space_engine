#include<uniforms.glslh>
#include<frag.glslh>

void main()
{
	out_color = color * texture(diffuse_texture, frag_uv);
}
