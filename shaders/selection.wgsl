// Block-selection wireframe shader.
//
// Draws 12 line segments around the selected block. Vertex positions are
// world-space coords with the chunk origin already baked in (via the
// per-call buffer write). Renders into the world's depth buffer using
// reversed-Z `Greater` with depth-write disabled — so solid geometry in
// front of the wireframe occludes it, but the wireframe itself doesn't
// disturb depth values used by subsequent passes.

struct FrameUniforms {
    view:        mat4x4<f32>,
    proj:        mat4x4<f32>,
    view_proj:   mat4x4<f32>,
    camera_pos:  vec4<f32>,
    sun_dir:     vec4<f32>,
    screen_size: vec2<f32>,
    time:        f32,
    fog_start:   f32,
    fog_end:     f32,
    _pad0:       f32,
    _pad1:       f32,
    _pad2:       f32,
};

@group(0) @binding(0) var<uniform> frame: FrameUniforms;

@vertex
fn vs_main(@location(0) position: vec3<f32>) -> @builtin(position) vec4<f32> {
    return frame.view_proj * vec4<f32>(position, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    // Output pure white. Paired with a `OneMinusDst` blend factor on the
    // pipeline this produces `1 - dst` — i.e. an inverted-color wireframe
    // that's always high-contrast against whatever the world pass drew
    // beneath it (bright on dark stone, dark on grass, etc.).
    return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
