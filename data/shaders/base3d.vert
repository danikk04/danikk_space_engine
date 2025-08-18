#include<uniforms.glslh>
#include<vert.glslh>

void main()
{
    frag_uv = vertex_uv;
    frag_normal = vertex_normal;
    gl_Position =  projection * view * world * vec4(vertex_pos, 1.0f);
}
