#include<uniforms.glslh>
#include<vert.glslh>

void main()
{
    frag_uv = vertex_uv;
    gl_Position =  projection * view * world * vec4(vertex_pos, 1.0f);
}
