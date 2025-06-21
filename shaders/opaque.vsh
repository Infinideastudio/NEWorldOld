#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in uvec3 a_color;
layout(location = 3) in ivec3 a_tangent;
layout(location = 4) in ivec3 a_bitangent;
layout(location = 5) in uint a_block_id;

out vec3 coord;
out vec3 tex_coord;
out vec3 color;
out vec3 tangent;
out vec3 bitangent;
flat out uint block_id;

layout(std140, row_major) uniform Frame {
    mat4 u_mvp;
    float u_game_time;
    vec3 u_sunlight_dir;
    float u_buffer_width;
    float u_buffer_height;
    float u_render_distance;

    mat4 u_shadow_mvp;
    float u_shadow_resolution;
    float u_shadow_fisheye_factor;
    float u_shadow_distance;

    int u_repeat_length;
    ivec3 u_player_coord_int;
    ivec3 u_player_coord_mod;
    vec3 u_player_coord_frac;
};

layout(std140, row_major) uniform Model {
    vec3 u_translation;
};

const float PI = 3.1415926f;
const uint LEAF_ID = 8u;

void main() {
    coord = vec3(a_coord);
    if (a_block_id == LEAF_ID) {
        float a = u_game_time * 0.2f;
        coord += vec3(sin(coord.x + a), sin(coord.y * 10.0f + a + PI / 3.0f * 2.0f), sin(coord.z * 10.0f + a + PI / 3.0f * 4.0f)) * 0.005f;
    }
    tex_coord = vec3(a_tex_coord);
    color = vec3(a_color) / 255.0;
    tangent = vec3(a_tangent);
    bitangent = vec3(a_bitangent);
    block_id = a_block_id;

    gl_Position = u_mvp * vec4(coord + u_translation, 1.0f);
}
