#version 330 core

layout(location = 0) in vec2 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in uvec4 a_color;

out vec3 tex_coord;
out vec4 color;

layout(std140, row_major) uniform Frame {
    mat4 u_mvp;
    float u_game_time;
    vec3 u_sunlight_dir;
    float u_buffer_width;
    float u_buffer_height;
    float u_render_distance;
};

void main() {
    tex_coord = vec3(a_tex_coord);
    color = vec4(a_color) / 255.0;
    gl_Position = vec4(a_coord.x / u_buffer_width * 2.0 - 1.0, 1.0 - a_coord.y / u_buffer_height * 2.0, 0.0, 1.0);
}
