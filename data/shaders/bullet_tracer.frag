#include<uniforms.glslh>
#include<frag.glslh>

/** @brief Outputs the tracer tint without applying scene lighting. */
void main()
{
	out_color = color * texture(diffuse_texture, frag_uv);
}
