#include<uniforms.glslh>
#include<vert.glslh>

layout(location = 10) uniform float terrain_depth_offset;

void main()
{
    frag_uv = vertex_uv;
    frag_normal = mat3(transpose(inverse(view * world))) * vertex_normal;
    gl_Position =  projection * view * world * vec4(vertex_pos, 1.0f);
    gl_Position.z = min(gl_Position.z + terrain_depth_offset * gl_Position.w,
        gl_Position.w - abs(gl_Position.w) * 1e-6f);
}
