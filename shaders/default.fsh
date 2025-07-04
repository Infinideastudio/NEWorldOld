#version 330 core

centroid in vec3 coord;
centroid in vec3 tex_coord;
centroid in vec3 color;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2DArray u_diffuse;
uniform sampler2DArray u_normal;

void main() {
    vec4 texel = texture(u_diffuse, tex_coord.stp);
    if (texel.a <= 0.0) discard;
    o_frag_color = vec4(color, 1.0) * texel;
}
