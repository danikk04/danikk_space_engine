#include<uniforms.glslh>
#include<uniform_textures.glslh>
#include<frag.glslh>

void main()
{
	out_color = color * texture(diffuse_texture, frag_uv);
}