#version 330 core

layout(location = 0) in vec2 a_coord;
layout(location = 1) in vec2 a_tex_coord;

out vec2 tex_coord;

layout(std140, row_major) uniform Filter {
    float u_buffer_width;
    float u_buffer_height;
    int u_filter_id;
    float u_gaussian_blur_radius;
    float u_gaussian_blur_step_size;
    float u_gaussian_blur_sigma; // Standard deviation
};

void main() {
    tex_coord = a_tex_coord;
    gl_Position = vec4(a_coord.x / u_buffer_width * 2.0 - 1.0, 1.0 - a_coord.y / u_buffer_height * 2.0, 0.0, 1.0);
}
