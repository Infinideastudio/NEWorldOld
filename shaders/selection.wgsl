// Line-overlay shader — handles both the block-selection wireframe
// (12-edge cube, world-projected, occluded by world geometry) and the
// center-screen crosshair (4-vert `+`, screen-space, always on top).
//
// Each logical line segment is rasterised as a 6-vertex quad (two
// triangles) so we can give the lines a configurable pixel thickness.
// `LineList` topology maps to a 1-device-pixel stroke on every backend
// wgpu targets — there's no portable `glLineWidth` analogue. The VS
// expands each vertex perpendicular to the projected segment direction
// by [`HALF_WIDTH_PX`] pixels, converted to NDC via `frame.screen_size`.
//
// Both kinds share:
//   * Pipeline state: TriangleList, OneMinusDst color blend, depth-write
//     off, depth-compare `Greater` (reversed-Z).
//   * Fragment output: pure white (the blend factor inverts against
//     the destination, producing a high-contrast stroke).
//
// They differ only in how the endpoints project to NDC, branched on
// `kind`:
//   * KIND_WORLD (0): `a` and `b` are world coords; the VS multiplies
//     each by `view_proj` and divides by w to land in NDC. The cube is
//     enlarged outward by the host-side `SELECTION_EPS` so its lines
//     clear the block face's exact reverse-Z value.
//   * KIND_SCREEN (1): `a.xy` and `b.xy` are pixel offsets from screen
//     centre, z unused. VS converts them to NDC directly and emits
//     clip-space `z = 1.0` (reversed-Z near plane); the depth buffer
//     never holds 1.0 in practice, so `Greater` always passes — same
//     effect the standalone pipeline got from `CompareFunction::Always`.

struct FrameUniforms {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    view_proj: mat4x4<f32>,
    inv_view_proj: mat4x4<f32>,
    shadow_view_proj: mat4x4<f32>,
    camera_pos: vec4<f32>,
    sun_dir: vec4<f32>,
    screen_size: vec2<f32>,
    time: f32,
    fog_start: f32,
    fog_end: f32,
    render_distance: f32,
    _pad_scalars: vec2<f32>,
    material_layers: vec4<u32>,
    shadow_params: vec4<f32>,
    player_coord_int: vec4<i32>,
    player_coord_mod: vec4<i32>,
    player_coord_frac: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> frame: FrameUniforms;

const KIND_WORLD: u32 = 0u;
const KIND_SCREEN: u32 = 1u;

/// Half the line thickness, in pixels. Total visible width = 2 *
/// HALF_WIDTH_PX. Tuned for readability — cube edges and crosshair arms
/// both render at this width.
const HALF_WIDTH_PX: f32 = 1.5;

/// Corner identifiers within a segment's quad. Bit 1 picks the segment
/// endpoint (A vs B); bit 0 picks the perpendicular side (+ vs −).
const CORNER_A_PLUS: u32 = 0u;
const CORNER_A_MINUS: u32 = 1u;
const CORNER_B_PLUS: u32 = 2u;
const CORNER_B_MINUS: u32 = 3u;

@vertex
fn vs_main(@location(0) a: vec3<f32>, @location(1) b: vec3<f32>, @location(2) corner: u32, @location(3) kind: u32,) -> @builtin(position) vec4<f32> {
    let is_b = (corner == CORNER_B_PLUS) || (corner == CORNER_B_MINUS);
    let side = select(- 1.0, 1.0, (corner == CORNER_A_PLUS) || (corner == CORNER_B_PLUS));

    if (kind == KIND_SCREEN) {
        // Endpoints in pixel offsets from screen centre.
        let a_ndc = a.xy * 2.0 / frame.screen_size;
        let b_ndc = b.xy * 2.0 / frame.screen_size;
        let endpoint_ndc = select(a_ndc, b_ndc, is_b);
        // Perpendicular in NDC, scaled so the pixel half-width matches
        // regardless of viewport size.
        let dir_ndc = b_ndc - a_ndc;
        let dir_pixel = dir_ndc * frame.screen_size * 0.5;
        let perp_pixel = normalize(vec2<f32>(- dir_pixel.y, dir_pixel.x));
        let extrude_ndc = perp_pixel * (HALF_WIDTH_PX * side) * 2.0 / frame.screen_size;
        return vec4<f32>(endpoint_ndc + extrude_ndc, 1.0, 1.0);
    }
    else {
        // World-space line. Both endpoints go through the projection so we
        // can compute the screen-space direction; the extrude is then
        // applied to the chosen endpoint in NDC.
        let clip_a = frame.view_proj * vec4<f32>(a, 1.0);
        let clip_b = frame.view_proj * vec4<f32>(b, 1.0);
        let ndc_a = clip_a.xy / clip_a.w;
        let ndc_b = clip_b.xy / clip_b.w;
        let endpoint_clip = select(clip_a, clip_b, is_b);
        let endpoint_ndc = endpoint_clip.xy / endpoint_clip.w;

        let dir_pixel = (ndc_b - ndc_a) * frame.screen_size * 0.5;
        let perp_pixel = normalize(vec2<f32>(- dir_pixel.y, dir_pixel.x));
        let extrude_ndc = perp_pixel * (HALF_WIDTH_PX * side) * 2.0 / frame.screen_size;
        let final_ndc = endpoint_ndc + extrude_ndc;

        // Re-multiply by the endpoint's clip w so post-divide x/y land at
        // `final_ndc`. z and w come from the original projection so depth
        // interpolates correctly across the quad — extruding in screen
        // space doesn't shift z, which is the visually correct behavior
        // (a 1-px-wide line at distance D should depth-test as if it were
        // exactly at D, not at D ± thickness/2).
        return vec4<f32>(final_ndc * endpoint_clip.w, endpoint_clip.z + 1e-4 * endpoint_clip.w, endpoint_clip.w);
    }
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    // Pipeline blend is `OneMinusDst / Zero / Add` so emitting pure
    // white gives `out = 1 - dst` — an inverted-color stroke that's
    // high-contrast against any backdrop.
    return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
