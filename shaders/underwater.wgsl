// Underwater full-screen overlay.
//
// Drawn after the world pass when the player's eye sits inside a water
// block — mirrors the C++ behaviour in `neworld.ixx:783-799`. Vertex
// position is generated procedurally from the vertex index; the fragment
// samples the water face from the block diffuse atlas and alpha-blends
// over the world. The water texture's per-pixel alpha is what produces
// the tint instead of a flat color.

struct OverlayUniforms {
    /// Atlas layer for the water top face. Pushed in once at world load.
    layer: u32,
    /// `0` if the player is dry and the overlay should be skipped, `1`
    /// when underwater. Lets the GPU degenerate the quad cheaply (we
    /// could also avoid binding the pipeline, but a single conditional
    /// here keeps the call sites simple).
    enabled: u32,
    _pad0: u32,
    _pad1: u32,
};

@group(0) @binding(0) var block_diffuse: texture_2d_array<f32>;
@group(0) @binding(1) var block_sampler: sampler;
@group(1) @binding(0) var<uniform> overlay: OverlayUniforms;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    // 6 vertices = 2 triangles covering the full clip rect. Vertex order:
    //   0: (-1, -1)   3: ( 1,  1)
    //   1: ( 1, -1)   4: (-1,  1)
    //   2: ( 1,  1)   5: (-1, -1)
    let positions = array<vec2<f32>, 6>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>( 1.0,  1.0),
        vec2<f32>( 1.0,  1.0),
        vec2<f32>(-1.0,  1.0),
        vec2<f32>(-1.0, -1.0),
    );
    let pos = positions[vid];
    var out: VsOut;
    if (overlay.enabled == 0u) {
        // Collapse to a degenerate point so the rasterizer emits no
        // fragments. Cheaper than gating the draw at the CPU level.
        out.clip_position = vec4<f32>(2.0, 2.0, 0.0, 1.0);
        out.uv = vec2<f32>(0.0, 0.0);
        return out;
    }
    out.clip_position = vec4<f32>(pos, 0.0, 1.0);
    // Map clip-space `[-1, 1]` to UV `[0, 1]`. The texture is `Repeat`-sampled
    // (atlases share the chunk sampler), so a single full-screen tile shows
    // one copy of the water face at the screen's aspect ratio — same look
    // the C++ overlay paints.
    out.uv = vec2<f32>(pos.x * 0.5 + 0.5, 1.0 - (pos.y * 0.5 + 0.5));
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, i32(overlay.layer));
    // Pre-blend tint so the world shows through with a clear bluish cast.
    // Source alpha is the water tile's per-pixel alpha (it's a translucent
    // block); multiplying by 0.6 dampens it so HUD remains legible.
    return vec4<f32>(sample.rgb, sample.a * 0.6);
}
