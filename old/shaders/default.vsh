#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in uvec3 a_color;

out vec3 coord;
out vec3 tex_coord;
out vec3 color;

layout(std140, row_major) uniform Frame {
    mat4 u_mvp;
    float u_game_time;
};

layout(std140, row_major) uniform Model {
    vec3 u_translation;
};

void main() {
    coord = vec3(a_coord);
    tex_coord = vec3(a_tex_coord);
    color = vec3(a_color) / 255.0;
    gl_Position = u_mvp * vec4(a_coord + u_translation, 1.0);
}
