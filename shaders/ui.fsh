#version 330 core

in vec3 tex_coord;
in vec4 color;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2DArray u_diffuse;

void main() {
    vec4 texel = texture(u_diffuse, tex_coord.stp);
    o_frag_color = color * texel;
}
