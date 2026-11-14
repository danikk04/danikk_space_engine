#include<uniforms.glslh>
#include<frag.glslh>

void main()
{
	vec4 surface_color = color * texture(diffuse_texture, frag_uv) * 3.0f;
	vec3 normal = normalize(frag_normal);
	vec3 light_direction = normalize(vec3(-0.4f, 0.7f, 0.6f));
	float diffuse = max(dot(normal, light_direction), 0.0f);
	float brightness = 0.25f + diffuse * 0.75f;
	out_color = vec4(surface_color.rgb * brightness, surface_color.a);
}
