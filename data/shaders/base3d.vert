#include<uniforms.glslh>
#include<vert.glslh>

void main()
{
    frag_uv = vertex_uv;
    frag_normal = mat3(transpose(inverse(view * world))) * vertex_normal;
    gl_Position =  projection * view * world * vec4(vertex_pos, 1.0f);
}
