#include<uniforms.glslh>
#include<frag.glslh>

void main()
{
	out_color = color * texture(diffuse_texture, frag_uv) * 3.0f - 0.5f;
}
