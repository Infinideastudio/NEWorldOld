#version 330 core

layout(location = 0) in vec2 a_coord;
layout(location = 1) in vec2 a_tex_coord;

out vec2 tex_coord;

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

void main() {
    tex_coord = a_tex_coord;
    gl_Position = vec4(a_coord.x / u_buffer_width * 2.0 - 1.0, 1.0 - a_coord.y / u_buffer_height * 2.0, 0.0, 1.0);
}
