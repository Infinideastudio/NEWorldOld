// Minimal scaffold pipeline: draws a colored full-viewport triangle from
// three hardcoded vertices, with no vertex buffers and no bind groups.
//
// Three positions are selected by `@builtin(vertex_index)`. Each corner
// gets a primary color (red / green / blue) and the rasterizer
// interpolates across the triangle's interior. The fragment stage just
// passes the interpolated color through to the color target.

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) color: vec3<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-0.8, -0.8),
        vec2<f32>( 0.8, -0.8),
        vec2<f32>( 0.0,  0.8),
    );
    var colors = array<vec3<f32>, 3>(
        vec3<f32>(1.0, 0.0, 0.0),
        vec3<f32>(0.0, 1.0, 0.0),
        vec3<f32>(0.0, 0.0, 1.0),
    );

    var out: VsOut;
    out.clip_position = vec4<f32>(positions[vid], 0.0, 1.0);
    out.color = colors[vid];
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color, 1.0);
}
