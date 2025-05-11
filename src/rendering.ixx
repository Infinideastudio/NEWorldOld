module;

#include <glad/gl.h>
#undef assert

export module rendering;
import std;
import types;
import debug;
import framebuffers;
import math;
import globals;
import render;

export namespace Renderer {
namespace spec = render::block_layout::spec;

using FilterUniformBlock = spec::Struct<
    spec::Field<"u_buffer_width", spec::Float>,
    spec::Field<"u_buffer_height", spec::Float>,
    spec::Field<"u_filter_id", spec::Int>,
    spec::Field<"u_gaussian_blur_radius", spec::Float>,
    spec::Field<"u_gaussian_blur_step_size", spec::Float>,
    spec::Field<"u_gaussian_blur_sigma", spec::Float>>;

using FrameUniformBlock = spec::Struct<
    spec::Field<"u_mvp", spec::Mat4f>,
    spec::Field<"u_game_time", spec::Float>,
    spec::Field<"u_sunlight_dir", spec::Vec3f>,
    spec::Field<"u_buffer_width", spec::Float>,
    spec::Field<"u_buffer_height", spec::Float>,
    spec::Field<"u_render_distance", spec::Float>,
    spec::Field<"u_shadow_mvp", spec::Mat4f>,
    spec::Field<"u_shadow_resolution", spec::Float>,
    spec::Field<"u_shadow_fisheye_factor", spec::Float>,
    spec::Field<"u_shadow_distance", spec::Float>,
    spec::Field<"u_repeat_length", spec::Int>,
    spec::Field<"u_player_coord_int", spec::Vec3i>,
    spec::Field<"u_player_coord_mod", spec::Vec3i>,
    spec::Field<"u_player_coord_frac", spec::Vec3f>>;

using ModelUniformBlock = spec::Struct<spec::Field<"u_translation", spec::Vec3f>>;

enum Shaders {
    UIShader,
    FilterShader,
    DefaultShader,
    OpqaueShader,
    TranslucentShader,
    FinalShader,
    ShadowShader,
    DebugShadowShader
};

double sunlightPitch = 30.0;
double sunlightHeading = 60.0;
std::vector<render::Program> shaders;

constexpr int gBufferCount = 3;
int gWidth, gHeight;
Framebuffer shadow, gBuffers, dBuffer;

auto filter_uniforms = render::Block<FilterUniformBlock>();
auto frame_uniforms = render::Block<FrameUniformBlock>();
auto model_uniforms = render::Block<ModelUniformBlock>();
auto filter_uniform_buffer = render::Buffer();
auto frame_uniform_buffer = render::Buffer();
auto model_uniform_buffer = render::Buffer();

auto getShadowDistance() -> int {
    return std::min(MaxShadowDistance, RenderDistance);
}

auto getNoiseTexture() -> GLuint {
    static GLuint noiseTex = 0;
    if (noiseTex == 0) {
        auto a = std::make_unique<uint8_t[]>(256 * 256 * 4);
        for (int i = 0; i < 256 * 256; i++)
            a[i * 4] = a[i * 4 + 1] = static_cast<uint8_t>(rnd() * 256);

        int const OffsetX = 37, OffsetY = 17;
        for (int x = 0; x < 256; x++)
            for (int y = 0; y < 256; y++) {
                int x1 = (x + OffsetX) % 256, y1 = (y + OffsetY) % 256;
                a[(y * 256 + x) * 4 + 2] = a[(y1 * 256 + x1) * 4];
                a[(y * 256 + x) * 4 + 3] = a[(y1 * 256 + x1) * 4 + 1];
            }

        glGenTextures(1, &noiseTex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, noiseTex);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 256, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, a.get());
    }
    return noiseTex;
}

void init_shaders(bool reload = false) {
    if (shaders.empty() || reload) {
        using render::Shader;
        using render::Program;
        using render::load_shader_source;

        auto defines = std::vector<std::string>{};
        if (MergeFace) {
            defines.emplace_back("MERGE_FACE");
        }
        if (SoftShadow) {
            defines.emplace_back("SOFT_SHADOW");
        }
        if (VolumetricClouds) {
            defines.emplace_back("VOLUMETRIC_CLOUDS");
        }
        if (AmbientOcclusion) {
            defines.emplace_back("AMBIENT_OCCLUSION");
        }
        shaders.clear();
        auto add_shader = [&](std::string_view vertex_path, std::string_view fragment_path) {
            auto vsh = *Shader::create(Shader::Stage::VERTEX, *load_shader_source(vertex_path, defines));
            auto fsh = *Shader::create(Shader::Stage::FRAGMENT, *load_shader_source(fragment_path, defines));
            shaders.emplace_back(*Program::create({std::move(vsh), std::move(fsh)}));
        };
        add_shader("shaders/ui.vsh", "shaders/ui.fsh");
        add_shader("shaders/filter.vsh", "shaders/filter.fsh");
        add_shader("shaders/default.vsh", "shaders/default.fsh");
        add_shader("shaders/opaque.vsh", "shaders/opaque.fsh");
        add_shader("shaders/translucent.vsh", "shaders/translucent.fsh");
        add_shader("shaders/final.vsh", "shaders/final.fsh");
        add_shader("shaders/shadow.vsh", "shaders/shadow.fsh");
        add_shader("shaders/debug_shadow.vsh", "shaders/debug_shadow.fsh");
    }

    // Create framebuffers.
    gWidth = WindowWidth;
    gHeight = WindowHeight;
    shadow = Framebuffer(ShadowRes, ShadowRes, 0, true, true);
    gBuffers = Framebuffer(gWidth, gHeight, gBufferCount, true, false);
    dBuffer = Framebuffer(gWidth, gHeight, 0, true, false);

    // Create uniform buffers.
    auto usage = render::Buffer::Usage::WRITE;
    auto update = render::Buffer::Update::INFREQUENT;
    filter_uniform_buffer = render::Buffer::create(filter_uniforms.bytes().size(), usage, update);
    frame_uniform_buffer = render::Buffer::create(frame_uniforms.bytes().size(), usage, update);
    model_uniform_buffer = render::Buffer::create(model_uniforms.bytes().size(), usage, update);

    // Set opaque uniforms and uniform buffer bindings.
    shaders[UIShader].set_opaque("u_diffuse", 0);
    shaders[UIShader].set_uniform_block("Frame", 0);

    shaders[FilterShader].set_opaque("u_buffer", 0);
    shaders[FilterShader].set_uniform_block("Filter", 0);

    shaders[DefaultShader].set_opaque("u_diffuse", 0);
    shaders[DefaultShader].set_uniform_block("Frame", 0);
    shaders[DefaultShader].set_uniform_block("Model", 1);

    shaders[OpqaueShader].set_opaque("u_diffuse", 0);
    shaders[OpqaueShader].set_uniform_block("Frame", 0);
    shaders[OpqaueShader].set_uniform_block("Model", 1);

    shaders[TranslucentShader].set_opaque("u_diffuse", 0);
    shaders[TranslucentShader].set_uniform_block("Frame", 0);
    shaders[TranslucentShader].set_uniform_block("Model", 1);

    shaders[FinalShader].set_opaque("u_diffuse_buffer", 0);
    shaders[FinalShader].set_opaque("u_normal_buffer", 1);
    shaders[FinalShader].set_opaque("u_material_buffer", 2);
    shaders[FinalShader].set_opaque("u_depth_buffer", gBufferCount + 0);
    shaders[FinalShader].set_opaque("u_shadow_texture", gBufferCount + 1);
    shaders[FinalShader].set_opaque("u_noise_texture", gBufferCount + 2);
    shaders[FinalShader].set_uniform_block("Frame", 0);
    shaders[FinalShader].set_uniform_block("Model", 1);

    shaders[ShadowShader].set_opaque("u_diffuse", 0);
    shaders[ShadowShader].set_uniform_block("Frame", 0);
    shaders[ShadowShader].set_uniform_block("Model", 1);

    shaders[DebugShadowShader].set_opaque("u_shadow_texture", 0);
}

auto getShadowMatrix() -> Mat4f {
    auto length = static_cast<float>(getShadowDistance() * 16);
    auto res = Mat4f(1.0f);
    res = Mat4f::rotate(-static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f)) * res;
    res = Mat4f::rotate(static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)) * res;
    res = Mat4f::ortho(-length, length, -length, length, -1000.0f, 1000.0f) * res;
    return res;
}

auto getShadowMatrixExperimental(float fov, float aspect, Eulerf orientation) -> Mat4f {
    auto length = static_cast<float>(getShadowDistance() * 16);
    auto res = Mat4f(1.0f);
    res = Mat4f::rotate(-static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f)) * res;
    res = Mat4f::rotate(static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)) * res;

    // Calculate view direction in light space, then rotate it to right (+1, 0)
    auto view_rotate = orientation.matrix();
    auto view_dir = (res * view_rotate).transform(Vec3f(0.0f, 0.0f, -1.0f));
    auto view_dir_xy = Vec3f(view_dir.x(), view_dir.y(), 0.0f);
    if (view_dir_xy.length() > 0.01f) {
        float radians = std::atan2(view_dir.y(), view_dir.x());
        res = Mat4f::rotate(-radians, Vec3f(0.0f, 0.0f, 1.0f)) * res;
    }

    // Minimal bounding box containing a diamond-shaped convex hull
    // (should approximate the visible parts better than the whole view frustum)
    float half_height = std::tan(static_cast<float>(fov * Pi / 180.0) / 2.0f);
    float half_width = half_height * static_cast<float>(aspect);
    auto vertices = std::array{
        Vec3f(0.0f, 0.0f, -1.0f),
        Vec3f(-half_width, -half_height, -1.0f),
        Vec3f(half_width, -half_height, -1.0f),
        Vec3f(half_width, half_height, -1.0f),
        Vec3f(-half_width, half_height, -1.0f),
    };
    auto to_light_space = res * view_rotate;
    float xmin = 0.0f, xmax = 0.0f, ymin = 0.0f, ymax = 0.0f;
    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].normalize();
        vertices[i] *= length;
        vertices[i] = to_light_space.transform(vertices[i]);
        xmin = std::min(xmin, vertices[i].x());
        xmax = std::max(xmax, vertices[i].x());
        ymin = std::min(ymin, vertices[i].y());
        ymax = std::max(ymax, vertices[i].y());
    }
    xmin = std::max(xmin, -length);
    xmax = std::min(xmax, length);
    ymin = std::max(ymin, -length);
    ymax = std::min(ymax, length);
    res = Mat4f::ortho(xmin, xmax, ymin, ymax, -1000.0f, 1000.0f) * res;
    return res;
}

void ClearSGDBuffers() {
    shadow.bindTargets();
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadow.unbindTarget();

    gBuffers.bindTargets();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gBuffers.unbindTarget();

    dBuffer.bindTargets();
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    dBuffer.unbindTarget();
}

void SetUniforms(Vec3d const& coord, Mat4f const& view_matrix, Mat4f const& shadow_matrix, float game_time) {
    // Calculate frame parameters.
    auto repeat = 25600;
    auto coord_int = coord.floor<int>();
    auto coord_mod = coord_int.map([=](auto x) { return x % repeat; });
    auto coord_frac = Vec3f(coord - Vec3d(coord_int));
    auto sunlight_dir = (Mat4f::rotate(static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f))
                         * Mat4f::rotate(-static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)))
                            .transform(Vec3f(0.0f, 0.0f, -1.0f));
    auto fisheye_factor = 0.8f;

    // Set frame uniforms.
    frame_uniforms.set<".u_mvp[0]">(view_matrix.row(0));
    frame_uniforms.set<".u_mvp[1]">(view_matrix.row(1));
    frame_uniforms.set<".u_mvp[2]">(view_matrix.row(2));
    frame_uniforms.set<".u_mvp[3]">(view_matrix.row(3));
    frame_uniforms.set<".u_game_time">(game_time);
    frame_uniforms.set<".u_sunlight_dir">(sunlight_dir);
    frame_uniforms.set<".u_buffer_width">(static_cast<float>(gWidth));
    frame_uniforms.set<".u_buffer_height">(static_cast<float>(gHeight));
    frame_uniforms.set<".u_render_distance">(static_cast<float>(RenderDistance) * 16.0f);

    frame_uniforms.set<".u_shadow_mvp[0]">(shadow_matrix.row(0));
    frame_uniforms.set<".u_shadow_mvp[1]">(shadow_matrix.row(1));
    frame_uniforms.set<".u_shadow_mvp[2]">(shadow_matrix.row(2));
    frame_uniforms.set<".u_shadow_mvp[3]">(shadow_matrix.row(3));
    frame_uniforms.set<".u_shadow_resolution">(static_cast<float>(ShadowRes));
    frame_uniforms.set<".u_shadow_fisheye_factor">(fisheye_factor);
    frame_uniforms.set<".u_shadow_distance">(static_cast<float>(getShadowDistance()) * 16.0f);

    frame_uniforms.set<".u_repeat_length">(repeat);
    frame_uniforms.set<".u_player_coord_int">(coord_int);
    frame_uniforms.set<".u_player_coord_mod">(coord_mod);
    frame_uniforms.set<".u_player_coord_frac">(coord_frac);

    // Set model uniforms.
    model_uniforms.set<".u_translation">({0.0f, 0.0f, 0.0f});

    // Update uniform buffer.
    frame_uniform_buffer.write(frame_uniforms.bytes(), 0);
    frame_uniform_buffer.bind(render::Buffer::IndexedTarget::UNIFORM, 0);
    model_uniform_buffer.write(model_uniforms.bytes(), 0);
    model_uniform_buffer.bind(render::Buffer::IndexedTarget::UNIFORM, 1);
}

void StartShadowPass() {
    assert(AdvancedRender);
    shadow.bindTargets();
    shaders[ShadowShader].bind();
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

void EndShadowPass() {
    assert(AdvancedRender);
    shadow.unbindTarget();
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void StartOpaquePass() {
    if (AdvancedRender) {
        gBuffers.bindTargets();
        shaders[OpqaueShader].bind();
    } else {
        shaders[DefaultShader].bind();
    }
    glDisable(GL_BLEND);
}

void EndOpaquePass() {
    if (AdvancedRender) {
        gBuffers.unbindTarget();
    }
    glEnable(GL_BLEND);
}

void StartTranslucentPass() {
    if (AdvancedRender) {
        // gBuffers.copyDepthTexture(dBuffer);
        gBuffers.bindTargets();
        shaders[TranslucentShader].bind();
    } else {
        shaders[DefaultShader].bind();
    }
}

void EndTranslucentPass() {
    if (AdvancedRender) {
        gBuffers.unbindTarget();
    }
}

void StartFinalPass() {
    assert(AdvancedRender);
    shaders[FinalShader].bind();
    gBuffers.bindColorTextures(0);
    gBuffers.bindDepthTexture(gBufferCount + 0);
    shadow.bindDepthTexture(gBufferCount + 1);
    glActiveTexture(GL_TEXTURE0 + gBufferCount + 2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, getNoiseTexture());
    glActiveTexture(GL_TEXTURE0);
}

void EndFinalPass() {
    assert(AdvancedRender);
}

}
