#version 330 core

in vec2 tex_coord;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2DArrayShadow u_shadow_texture;

float sample_shadow(vec2 pos) {
    float first = 0.0, last = 1.0, mid;
    for (int i = 0; i < 8; i++) {
        mid = (first + last) / 2.0;
        if (texture(u_shadow_texture, vec4(pos, 0.0, mid)) < 0.5) first = mid; else last = mid;
    }
    return mid;
}

void main() {
    float texel = sample_shadow(tex_coord);
    if (texel < 1.0 / 255.0) {
        o_frag_color = vec4(0.2, 0.2, 0.2, 0.5);
    } else {
        o_frag_color = vec4(texel, texel, texel, 1.0);
    }
}
