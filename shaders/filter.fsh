#version 330 core

centroid in vec2 tex_coord;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2DArray u_buffer;

layout(std140, row_major) uniform Filter {
    float u_buffer_width;
    float u_buffer_height;
    int u_filter_id;
    float u_gaussian_blur_radius;
    float u_gaussian_blur_step_size;
    float u_gaussian_blur_sigma; // Standard deviation
};

const float PI = 3.141593;

vec4 get_color(vec2 tex_coord) {
    return texture(u_buffer, vec3(tex_coord, 0.0));
}

void main() {
    if (u_filter_id == 1 || u_filter_id == 2) { // Gaussian blur
        float radius = u_gaussian_blur_radius;
        float step = u_gaussian_blur_step_size;
        float sigma2 = u_gaussian_blur_sigma * u_gaussian_blur_sigma;
        bool horizontal = (u_filter_id == 1);
        vec4 sum = vec4(0.0);
        float total = 0.0;
        for (float i = -radius; i <= radius; i += step) {
            float x = float(i);
            float weight = 1.0 / sqrt(2 * PI * sigma2) * exp(-(x * x) / (2 * sigma2));
            vec2 sample_coord = tex_coord;
            if (horizontal) sample_coord.x += x / u_buffer_width; else sample_coord.y += x / u_buffer_height;
            sum += weight * get_color(sample_coord);
            total += weight;
        }
        o_frag_color = sum / total;
    } else {
        o_frag_color = vec4(0.0);
    }
}
