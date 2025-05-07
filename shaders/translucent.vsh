#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in uvec3 a_color;
layout(location = 3) in ivec3 a_normal;
layout(location = 4) in uint a_block_id;

out vec3 coord;
out vec3 tex_coord;
out vec3 color;
out vec3 normal;
flat out uint block_id;

uniform mat4 u_mvp;
uniform vec3 u_translation;
uniform float u_game_time;

void main() {
    coord = vec3(a_coord);
    tex_coord = vec3(a_tex_coord);
    color = vec3(a_color) / 255.0;
    normal = vec3(a_normal);
    block_id = a_block_id;

    gl_Position = u_mvp * vec4(coord + u_translation, 1.0f);
}
