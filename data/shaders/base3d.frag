#include<uniforms.glslh>
#include<frag.glslh>

layout(location = 9) uniform bool alpha_discard_enabled;

/**
 * @brief Applies a solid-style directional diffuse shade to the sampled material color.
 * The normalized interpolated normal is lit from a fixed direction. Brightness
 * ranges from 0.25 ambient to 1.0 at full diffuse alignment; texture and vertex
 * tint provide the surface color, including their alpha channel.
 */
void main()
{
	vec4 texel_color = texture(diffuse_texture, frag_uv);
	if(alpha_discard_enabled && texel_color.a < 0.5f) discard;
	vec4 surface_color = color * texel_color;
	vec3 normal = normalize(frag_normal);
	vec3 light_direction = normalize(vec3(-0.4f, 0.7f, 0.6f));
	float diffuse = max(dot(normal, light_direction), 0.0f);
	float brightness = 0.25f + diffuse * 0.75f;
	out_color = vec4(surface_color.rgb * brightness, surface_color.a);
}
