// Skybox shader for the out-of-game menu background.
//
// Renders a unit cube around the camera (origin) sampled from a cubemap;
// the result is written to an offscreen target that the host then runs a
// separable Gaussian blur over before presenting. The cube slowly rotates
// via the time uniform so the menu has gentle motion behind it.
//
// The rotation is computed inside the vertex shader from `u.params.x`
// (wall-clock seconds since boot) — keeping it in WGSL means the host
// only needs to push `view_proj` + `time` and there is no host-side
// `mat4x4` to marshal, which used to bug out under some bind-group
// layouts.
//
// Faces are wound in [+X, -X, +Y, -Y, +Z, -Z] order to match the
// [`AtlasCube`] layer convention. Cull is disabled at the pipeline level
// so the back faces (the inner skybox surfaces visible from origin) draw
// regardless of winding.

const PI: f32 = 3.141592653589793;
const TAU: f32 = 6.283185307179586;

// Tuning constants — kept here (not host-side) so the shader is the
// single source of truth for the panorama motion. Same shape as
// Minecraft's title-screen panorama: continuous yaw around world-Y
// combined with a small sinusoidal pitch around world-X, biased a few
// degrees below the horizon so the horizon line subtly drifts up and
// down as the pitch oscillates.
const YAW_SPEED_RAD_PER_SEC: f32 = 0.0524;        // 3°/sec
const PITCH_BIAS_RAD: f32 = 0.2618;               // 15°
const PITCH_AMPLITUDE_RAD: f32 = 0.0873;          // 5°
const PITCH_PERIOD_SEC: f32 = 60.0;

struct Uniforms {
    view_proj: mat4x4<f32>,
    /// `params.x` is wall-clock seconds since the menu opened; the
    /// other components are reserved for future use (zeroed today).
    params: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(1) @binding(0) var sky: texture_cube<f32>;
@group(1) @binding(1) var samp: sampler;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) dir: vec3<f32>,
};

fn rotate_y(v: vec3<f32>, theta: f32) -> vec3<f32> {
    let c = cos(theta);
    let s = sin(theta);
    return vec3<f32>(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

fn rotate_x(v: vec3<f32>, theta: f32) -> vec3<f32> {
    let c = cos(theta);
    let s = sin(theta);
    return vec3<f32>(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    // 36-vertex unit cube, two triangles per face, faces in
    // [+X, -X, +Y, -Y, +Z, -Z] order.
    let positions = array<vec3<f32>, 36>(
        // +X
        vec3<f32>( 1.0, -1.0, -1.0), vec3<f32>( 1.0, -1.0,  1.0), vec3<f32>( 1.0,  1.0,  1.0),
        vec3<f32>( 1.0, -1.0, -1.0), vec3<f32>( 1.0,  1.0,  1.0), vec3<f32>( 1.0,  1.0, -1.0),
        // -X
        vec3<f32>(-1.0, -1.0,  1.0), vec3<f32>(-1.0, -1.0, -1.0), vec3<f32>(-1.0,  1.0, -1.0),
        vec3<f32>(-1.0, -1.0,  1.0), vec3<f32>(-1.0,  1.0, -1.0), vec3<f32>(-1.0,  1.0,  1.0),
        // +Y
        vec3<f32>(-1.0,  1.0, -1.0), vec3<f32>( 1.0,  1.0, -1.0), vec3<f32>( 1.0,  1.0,  1.0),
        vec3<f32>(-1.0,  1.0, -1.0), vec3<f32>( 1.0,  1.0,  1.0), vec3<f32>(-1.0,  1.0,  1.0),
        // -Y
        vec3<f32>(-1.0, -1.0,  1.0), vec3<f32>( 1.0, -1.0,  1.0), vec3<f32>( 1.0, -1.0, -1.0),
        vec3<f32>(-1.0, -1.0,  1.0), vec3<f32>( 1.0, -1.0, -1.0), vec3<f32>(-1.0, -1.0, -1.0),
        // +Z
        vec3<f32>( 1.0, -1.0,  1.0), vec3<f32>(-1.0, -1.0,  1.0), vec3<f32>(-1.0,  1.0,  1.0),
        vec3<f32>( 1.0, -1.0,  1.0), vec3<f32>(-1.0,  1.0,  1.0), vec3<f32>( 1.0,  1.0,  1.0),
        // -Z
        vec3<f32>(-1.0, -1.0, -1.0), vec3<f32>( 1.0, -1.0, -1.0), vec3<f32>( 1.0,  1.0, -1.0),
        vec3<f32>(-1.0, -1.0, -1.0), vec3<f32>( 1.0,  1.0, -1.0), vec3<f32>(-1.0,  1.0, -1.0),
    );

    let p = positions[vid];

    // Compose pitch-X * yaw-Y on the cube vertex so the visible
    // rotation axis tilts off vertical — matches the Minecraft
    // panorama feel.
    let t = u.params.x;
    let yaw = t * YAW_SPEED_RAD_PER_SEC;
    let pitch_phase = t * (TAU / PITCH_PERIOD_SEC);
    let pitch = sin(pitch_phase) * PITCH_AMPLITUDE_RAD + PITCH_BIAS_RAD;

    let yawed = rotate_y(p, yaw);
    let world_p = rotate_x(yawed, pitch);

    // The screen position uses the *rotated* cube vertex, but the
    // cubemap sample direction is the *un-rotated* vertex. Why: with
    // the camera at origin, every point on the rotated cube along a
    // given camera ray normalizes to the same direction (the camera
    // ray itself), so sampling by the rotated point gives a constant
    // colour per screen pixel and the panorama looks frozen. Using
    // the un-rotated `p` instead means each cube triangle carries the
    // un-rotated face direction with it as it spins; at any fixed
    // screen pixel a different triangle (and therefore a different
    // sample direction) lands there over time, so the sky drifts.
    var out: VsOut;
    out.clip_position = u.view_proj * vec4<f32>(world_p, 1.0);
    out.dir = p;
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let color = textureSample(sky, samp, normalize(in.dir));
    return vec4<f32>(color.rgb, 1.0);
}
