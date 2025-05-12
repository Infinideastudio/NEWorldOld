module;

#include <glad/gl.h>
#undef assert

export module rendering;
import std;
import types;
import debug;
import math;
import globals;
import render;

export namespace Renderer {
namespace bl = render::block_layout::spec;

using FilterUniformBlock = bl::Struct<
    bl::Field<"u_buffer_width", bl::Float>,
    bl::Field<"u_buffer_height", bl::Float>,
    bl::Field<"u_filter_id", bl::Int>,
    bl::Field<"u_gaussian_blur_radius", bl::Float>,
    bl::Field<"u_gaussian_blur_step_size", bl::Float>,
    bl::Field<"u_gaussian_blur_sigma", bl::Float>>;

using FrameUniformBlock = bl::Struct<
    bl::Field<"u_mvp", bl::Mat4f>,
    bl::Field<"u_game_time", bl::Float>,
    bl::Field<"u_sunlight_dir", bl::Vec3f>,
    bl::Field<"u_buffer_width", bl::Float>,
    bl::Field<"u_buffer_height", bl::Float>,
    bl::Field<"u_render_distance", bl::Float>,
    bl::Field<"u_shadow_mvp", bl::Mat4f>,
    bl::Field<"u_shadow_resolution", bl::Float>,
    bl::Field<"u_shadow_fisheye_factor", bl::Float>,
    bl::Field<"u_shadow_distance", bl::Float>,
    bl::Field<"u_repeat_length", bl::Int>,
    bl::Field<"u_player_coord_int", bl::Vec3i>,
    bl::Field<"u_player_coord_mod", bl::Vec3i>,
    bl::Field<"u_player_coord_frac", bl::Vec3f>>;

using ModelUniformBlock = bl::Struct<bl::Field<"u_translation", bl::Vec3f>>;

enum Shaders {
    UIShader,
    FilterShader,
    DefaultShader,
    OpqaueShader,
    TranslucentShader,
    FinalShader,
    ShadowShader,
    DebugShadowShader,
};

enum Textures {
    DiffuseTexture,
    NormalTexture,
    MaterialTexture,
    DepthTexture,
    ShadowColorTexture,
    ShadowDepthTexture,
    NoiseTexture,
};

enum Framebuffers {
    Deferred,
    Shadow,
};

size_t bufferWidth = 0;
size_t bufferHeight = 0;
double sunlightPitch = 30.0;
double sunlightHeading = 60.0;
std::vector<render::Program> shaders;
std::vector<render::Texture> textures;
std::vector<render::Framebuffer> framebuffers;

auto filter_uniforms = render::Block<FilterUniformBlock>();
auto frame_uniforms = render::Block<FrameUniformBlock>();
auto model_uniforms = render::Block<ModelUniformBlock>();
auto filter_uniform_buffer = render::Buffer();
auto frame_uniform_buffer = render::Buffer();
auto model_uniform_buffer = render::Buffer();

auto ui_vertex_builder() {
    namespace al = render::attrib_layout::spec;
    return render::AttribIndexBuilder<al::Coord<al::Vec2f>, al::TexCoord<al::Vec3f>, al::Color<al::Vec4u8>>();
}

auto chunk_vertex_builder() {
    namespace al = render::attrib_layout::spec;
    return render::AttribIndexBuilder<
        al::Coord<al::Vec3f>,
        al::TexCoord<al::Vec3f>,
        al::Color<al::Vec3u8>,
        al::Normal<al::Vec3i8>,
        al::Material<al::UInt16>>();
}

auto shadow_distance() -> int {
    return std::min(MaxShadowDistance, RenderDistance);
}

auto create_noise_texture() -> render::Texture {
    fast_srand(1234);
    auto image = render::ImageRGBA(1, 256, 256);
    for (auto i = 0uz; i < 256; i++)
        for (auto j = 0uz; j < 256; j++)
            image[0, i, j].x() = image[0, i, j].y() = static_cast<uint8_t>(rnd() * 256);

    auto OFFSET_X = 37uz, OFFSET_Y = 17uz;
    for (auto i = 0; i < 256uz; i++) {
        for (auto j = 0; j < 256uz; j++) {
            auto k = (i + OFFSET_Y) % 256;
            auto l = (j + OFFSET_X) % 256;
            image[0, i, j].z() = image[0, k, l].x();
            image[0, i, j].w() = image[0, k, l].y();
        }
    }

    auto tex = render::Texture::create(render::Texture::Format::RGBA, 1, 256, 256);
    tex.fill(0, 0, 0, image);
    tex.set_wrap(true);
    tex.set_filter(true);
    return std::move(tex);
}

void init_pipeline(bool load_shaders = false, bool init_framebuffers = false) {
    // Create shaders.
    if (load_shaders) {
        shaders.clear();

        using render::Shader;
        using render::Program;
        using render::Buffer;
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
        shaders[FinalShader].set_opaque("u_depth_buffer", 3);
        shaders[FinalShader].set_opaque("u_shadow_texture", 4);
        shaders[FinalShader].set_opaque("u_noise_texture", 5);
        shaders[FinalShader].set_uniform_block("Frame", 0);
        shaders[FinalShader].set_uniform_block("Model", 1);

        shaders[ShadowShader].set_opaque("u_diffuse", 0);
        shaders[ShadowShader].set_uniform_block("Frame", 0);
        shaders[ShadowShader].set_uniform_block("Model", 1);

        shaders[DebugShadowShader].set_opaque("u_shadow_texture", 0);

        filter_uniform_buffer =
            Buffer::create(filter_uniforms.bytes().size(), Buffer::Usage::WRITE, Buffer::Update::SEMI_FREQUENT);
        frame_uniform_buffer =
            Buffer::create(frame_uniforms.bytes().size(), Buffer::Usage::WRITE, Buffer::Update::SEMI_FREQUENT);
        model_uniform_buffer =
            Buffer::create(model_uniforms.bytes().size(), Buffer::Usage::WRITE, Buffer::Update::SEMI_FREQUENT);
    }

    // Create framebuffers.
    if (init_framebuffers) {
        textures.clear();
        framebuffers.clear();

        using render::Texture;
        using render::Framebuffer;

        bufferWidth = WindowWidth;
        bufferHeight = WindowHeight;

        textures.emplace_back(Texture::create(Texture::Format::RGBA, 1, bufferHeight, bufferWidth));
        textures.emplace_back(Texture::create(Texture::Format::RGBA, 1, bufferHeight, bufferWidth));
        textures.emplace_back(Texture::create(Texture::Format::RGBA, 1, bufferHeight, bufferWidth));
        textures.emplace_back(Texture::create(Texture::Format::DEPTH, 1, bufferHeight, bufferWidth));
        textures.emplace_back(Texture::create(Texture::Format::RGBA, 1, ShadowRes, ShadowRes));
        textures.emplace_back(Texture::create(Texture::Format::DEPTH, 1, ShadowRes, ShadowRes));
        textures.back().set_filter(true);
        textures.back().set_depth_compare_mode(Texture::DepthCompareMode::LEQUAL);
        textures.emplace_back(create_noise_texture());

        framebuffers.emplace_back(*Framebuffer::create(
            {
                Framebuffer::Attachment{ .texture = textures[DiffuseTexture], .mipmap_level = 0, .layer = 0},
                Framebuffer::Attachment{  .texture = textures[NormalTexture], .mipmap_level = 0, .layer = 0},
                Framebuffer::Attachment{.texture = textures[MaterialTexture], .mipmap_level = 0, .layer = 0},
        },
            Framebuffer::Attachment{.texture = textures[DepthTexture], .mipmap_level = 0, .layer = 0},
            {}
        ));
        framebuffers.emplace_back(*Framebuffer::create(
            {
                Framebuffer::Attachment{.texture = textures[ShadowColorTexture], .mipmap_level = 0, .layer = 0},
        },
            Framebuffer::Attachment{.texture = textures[ShadowDepthTexture], .mipmap_level = 0, .layer = 0},
            {}
        ));

        Framebuffer::bind_default(render::Framebuffer::Target::WRITE);
        glViewport(0, 0, WindowWidth, WindowHeight);
    }
}

auto getShadowMatrix() -> Mat4f {
    auto length = static_cast<float>(shadow_distance() * 16);
    auto res = Mat4f(1.0f);
    res = Mat4f::rotate(-static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f)) * res;
    res = Mat4f::rotate(static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)) * res;
    res = Mat4f::ortho(-length, length, -length, length, -1000.0f, 1000.0f) * res;
    return res;
}

auto getShadowMatrixExperimental(float fov, float aspect, Eulerf orientation) -> Mat4f {
    auto length = static_cast<float>(shadow_distance() * 16);
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
    framebuffers[Shadow].bind(render::Framebuffer::Target::WRITE);
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    framebuffers[Deferred].bind(render::Framebuffer::Target::WRITE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
    frame_uniforms.set<".u_buffer_width">(static_cast<float>(bufferWidth));
    frame_uniforms.set<".u_buffer_height">(static_cast<float>(bufferHeight));
    frame_uniforms.set<".u_render_distance">(static_cast<float>(RenderDistance) * 16.0f);

    frame_uniforms.set<".u_shadow_mvp[0]">(shadow_matrix.row(0));
    frame_uniforms.set<".u_shadow_mvp[1]">(shadow_matrix.row(1));
    frame_uniforms.set<".u_shadow_mvp[2]">(shadow_matrix.row(2));
    frame_uniforms.set<".u_shadow_mvp[3]">(shadow_matrix.row(3));
    frame_uniforms.set<".u_shadow_resolution">(static_cast<float>(ShadowRes));
    frame_uniforms.set<".u_shadow_fisheye_factor">(fisheye_factor);
    frame_uniforms.set<".u_shadow_distance">(static_cast<float>(shadow_distance()) * 16.0f);

    frame_uniforms.set<".u_repeat_length">(repeat);
    frame_uniforms.set<".u_player_coord_int">(coord_int);
    frame_uniforms.set<".u_player_coord_mod">(coord_mod);
    frame_uniforms.set<".u_player_coord_frac">(coord_frac);

    // Set model uniforms.
    model_uniforms.set<".u_translation">(0.0f, 0.0f, 0.0f);

    // Update uniform buffer.
    frame_uniform_buffer.write(frame_uniforms.bytes(), 0);
    frame_uniform_buffer.bind(render::Buffer::IndexedTarget::UNIFORM, 0);
    model_uniform_buffer.write(model_uniforms.bytes(), 0);
    model_uniform_buffer.bind(render::Buffer::IndexedTarget::UNIFORM, 1);
}

void StartShadowPass() {
    assert(AdvancedRender);
    framebuffers[Shadow].bind(render::Framebuffer::Target::WRITE);
    shaders[ShadowShader].bind();
    glViewport(0, 0, ShadowRes, ShadowRes);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

void EndShadowPass() {
    assert(AdvancedRender);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void StartOpaquePass() {
    if (AdvancedRender) {
        framebuffers[Deferred].bind(render::Framebuffer::Target::WRITE);
        shaders[OpqaueShader].bind();
    } else {
        render::Framebuffer::bind_default(render::Framebuffer::Target::WRITE);
        shaders[DefaultShader].bind();
    }
    glViewport(0, 0, static_cast<GLsizei>(bufferWidth), static_cast<GLsizei>(bufferHeight));
    glDisable(GL_BLEND);
}

void EndOpaquePass() {
    glEnable(GL_BLEND);
}

void StartTranslucentPass() {
    if (AdvancedRender) {
        framebuffers[Deferred].bind(render::Framebuffer::Target::WRITE);
        shaders[TranslucentShader].bind();
    } else {
        render::Framebuffer::bind_default(render::Framebuffer::Target::WRITE);
        shaders[DefaultShader].bind();
    }
    glViewport(0, 0, static_cast<GLsizei>(bufferWidth), static_cast<GLsizei>(bufferHeight));
}

void EndTranslucentPass() {}

void StartFinalPass() {
    assert(AdvancedRender);
    render::Framebuffer::bind_default(render::Framebuffer::Target::WRITE);
    shaders[FinalShader].bind();
    textures[DiffuseTexture].bind(0);
    textures[NormalTexture].bind(1);
    textures[MaterialTexture].bind(2);
    textures[DepthTexture].bind(3);
    textures[ShadowDepthTexture].bind(4);
    textures[NoiseTexture].bind(5);
    glViewport(0, 0, static_cast<GLsizei>(bufferWidth), static_cast<GLsizei>(bufferHeight));
}

void EndFinalPass() {
    assert(AdvancedRender);
}

}
