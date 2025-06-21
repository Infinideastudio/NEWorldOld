#version 330 core

centroid in vec3 coord;
centroid in vec3 tex_coord;
centroid in vec3 color;
centroid in vec3 tangent;
centroid in vec3 bitangent;
flat in uint block_id;

layout(location = 0) out vec4 o_frag_color;
layout(location = 1) out vec4 o_normal;
layout(location = 2) out vec4 o_block_id;

uniform sampler2DArray u_diffuse;
uniform sampler2DArray u_normal;

vec2 encode_u16(uint v) {
    uint high = v / 256u;
    uint low = v - high * 256u;
    return vec2(float(high) / 255.0, float(low) / 255.0);
}

void main() {
    vec4 texel = texture(u_diffuse, tex_coord.stp);
    if (texel.a <= 0.0) discard;
    texel.a = 1.0;

    vec3 normal_texel = texture(u_normal, tex_coord.stp).rgb * 2.0 - vec3(1.0);
    mat3 tbn = mat3(normalize(tangent), normalize(bitangent), normalize(cross(tangent, bitangent)));
    vec3 normal = tbn * normalize(normal_texel);

    o_frag_color = vec4(color, 1.0) * texel;
    o_normal = vec4(normal * 0.5 + vec3(0.5), 1.0);
    o_block_id = vec4(encode_u16(block_id), 0.0, 1.0);
}
