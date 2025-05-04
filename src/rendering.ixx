module;

#include <cassert>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <glad/gl.h>

export module rendering;
import shaders;
import framebuffers;
import mat4;
import vec3;
import globals;

export namespace Renderer {

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

constexpr size_t ArraySize = 4194304;

GLenum Primitive;
int Vertexes, Coordc, Texcoordc, Colorc, Normalc, Attribc;
float* VertexArray = nullptr;
float Coords[4], TexCoords[4], Colors[4], Normals[4], Attribs[4];
double sunlightPitch = 30.0;
double sunlightHeading = 60.0;
std::vector<Shader> shaders;
int ActiveShader;
int index = 0, size = 0;

constexpr int gBufferCount = 3;
int gWidth, gHeight;
Framebuffer shadow, gBuffers, dBuffer;

class VertexBuffer {
public:
    VertexBuffer():
        vao(0),
        vbo(0),
        primitive(GL_TRIANGLES),
        numVertices(0) {}
    VertexBuffer(VertexBuffer const&) = delete;
    VertexBuffer(VertexBuffer&& from) noexcept:
        VertexBuffer() {
        swap(*this, from);
    }
    auto operator=(VertexBuffer const&) -> VertexBuffer& = delete;
    auto operator=(VertexBuffer&& from) noexcept -> VertexBuffer& {
        swap(*this, from);
        return *this;
    }

    ~VertexBuffer() {
        if (vbo != 0)
            glDeleteBuffers(1, &vbo);
        if (vao != 0)
            glDeleteVertexArrays(1, &vao);
    }

    friend void swap(VertexBuffer& first, VertexBuffer& second) noexcept {
        using std::swap;
        swap(first.vao, second.vao);
        swap(first.vbo, second.vbo);
        swap(first.primitive, second.primitive);
        swap(first.numVertices, second.numVertices);
    }

    static VertexBuffer upload(bool staticDraw);
    bool empty() const {
        return vao == 0 || vbo == 0 || numVertices == 0;
    }
    void render() const;

private:
    GLuint vao;
    GLuint vbo;
    GLenum primitive;
    GLuint numVertices;
};

void Begin(GLenum primitive, int coords, int texCoords, int colors, int normals = 0, int attributes = 0);
void Vertex2i(int x, int y);
void Vertex2f(float x, float y);
void Vertex3f(float x, float y, float z);
void TexCoord2f(float x, float y);
void TexCoord3f(float x, float y, float z);
void Color3f(float r, float g, float b);
void Color4f(float r, float g, float b, float a);
void Normal3f(float x, float y, float z);
void Attrib1f(float attr);

inline auto End(bool staticDraw = false) -> VertexBuffer {
    return VertexBuffer::upload(staticDraw);
}

void initShaders(bool reload = false);
inline void bindShader(int shaderID) {
    shaders[shaderID].bind();
    ActiveShader = shaderID;
}

inline auto getShadowDistance() -> int {
    return std::min(MaxShadowDistance, RenderDistance);
}
auto getShadowMatrix() -> Mat4f;
auto getShadowMatrixExperimental(float fov, float aspect, double heading, double pitch) -> Mat4f;

void ClearSGDBuffers();
void StartShadowPass(Mat4f const& shadowMatrix, float gameTime);
void EndShadowPass();
void StartOpaquePass(Mat4f const& viewMatrix, float gametime);
void EndOpaquePass();
void StartTranslucentPass(Mat4f const& viewMatrix, float gametime);
void EndTranslucentPass();
void StartFinalPass(
    double xpos,
    double ypos,
    double zpos,
    Mat4f const& viewMatrix,
    Mat4f const& shadowMatrix,
    float gameTime
);
void EndFinalPass();

void Begin(GLenum primitive, int coords, int texCoords, int colors, int normals, int attributes) {
    Primitive = primitive;
    Coordc = coords;
    Texcoordc = texCoords;
    Colorc = colors;
    Normalc = normals;
    Attribc = attributes;
    if (VertexArray == nullptr)
        VertexArray = new float[ArraySize];
    index = 0;
    Vertexes = 0;
    size = (Coordc + Texcoordc + Colorc + Normalc + Attribc) * 4;
}

void addVertex() {
    if ((Vertexes + 1) * (Coordc + Texcoordc + Colorc + Normalc + Attribc) > ArraySize)
        return;
    Vertexes++;
    if (Coordc != 0)
        memcpy(VertexArray + index, Coords, Coordc * sizeof(float));
    index += Coordc;
    if (Texcoordc != 0)
        memcpy(VertexArray + index, TexCoords, Texcoordc * sizeof(float));
    index += Texcoordc;
    if (Colorc != 0)
        memcpy(VertexArray + index, Colors, Colorc * sizeof(float));
    index += Colorc;
    if (Normalc != 0)
        memcpy(VertexArray + index, Normals, Normalc * sizeof(float));
    index += Normalc;
    if (Attribc != 0)
        memcpy(VertexArray + index, Attribs, Attribc * sizeof(float));
    index += Attribc;
}

void Vertex2i(int x, int y) {
    Vertex2f(static_cast<float>(x), static_cast<float>(y));
}
void Vertex1f(float x) {
    Coords[0] = x;
    addVertex();
}
void Vertex2f(float x, float y) {
    Coords[0] = x;
    Coords[1] = y;
    addVertex();
}
void Vertex3f(float x, float y, float z) {
    Coords[0] = x;
    Coords[1] = y;
    Coords[2] = z;
    addVertex();
}
void Vertex4f(float x, float y, float z, float w) {
    Coords[0] = x;
    Coords[1] = y;
    Coords[2] = z;
    Coords[3] = w;
    addVertex();
}
void TexCoord1f(float s) {
    TexCoords[0] = s;
}
void TexCoord2f(float s, float t) {
    TexCoords[0] = s;
    TexCoords[1] = t;
}
void TexCoord3f(float s, float t, float u) {
    TexCoords[0] = s;
    TexCoords[1] = t;
    TexCoords[2] = u;
}
void TexCoord4f(float s, float t, float u, float v) {
    TexCoords[0] = s;
    TexCoords[1] = t;
    TexCoords[2] = u;
    TexCoords[3] = v;
}
void Color1f(float r) {
    Colors[0] = r;
}
void Color2f(float r, float g) {
    Colors[0] = r;
    Colors[1] = g;
}
void Color3f(float r, float g, float b) {
    Colors[0] = r;
    Colors[1] = g;
    Colors[2] = b;
}
void Color4f(float r, float g, float b, float a) {
    Colors[0] = r;
    Colors[1] = g;
    Colors[2] = b;
    Colors[3] = a;
}
void Normal3f(float x, float y, float z) {
    Normals[0] = x;
    Normals[1] = y;
    Normals[2] = z;
}
void Attrib1f(float a) {
    Attribs[0] = a;
}

GLuint getNoiseTexture() {
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
        glBindTexture(GL_TEXTURE_2D, noiseTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, a.get());
    }
    return noiseTex;
}

void initShaders(bool reload) {
    if (shaders.empty() || reload) {
        std::vector<std::string> defines;
        if (MergeFace)
            defines.emplace_back("MERGE_FACE");
        if (SoftShadow)
            defines.emplace_back("SOFT_SHADOW");
        if (VolumetricClouds)
            defines.emplace_back("VOLUMETRIC_CLOUDS");
        if (AmbientOcclusion)
            defines.emplace_back("AMBIENT_OCCLUSION");
        shaders.clear();
        shaders.emplace_back("shaders/ui.vsh", "shaders/ui.fsh", defines);
        shaders.emplace_back("shaders/filter.vsh", "shaders/filter.fsh", defines);
        shaders.emplace_back("shaders/default.vsh", "shaders/default.fsh", defines);
        shaders.emplace_back("shaders/opaque.vsh", "shaders/opaque.fsh", defines);
        shaders.emplace_back("shaders/translucent.vsh", "shaders/translucent.fsh", defines);
        shaders.emplace_back("shaders/final.vsh", "shaders/final.fsh", defines);
        shaders.emplace_back("shaders/shadow.vsh", "shaders/shadow.fsh", defines);
        shaders.emplace_back("shaders/debug_shadow.vsh", "shaders/debug_shadow.fsh", defines);
    }

    // Create framebuffers
    gWidth = WindowWidth;
    gHeight = WindowHeight;
    shadow = Framebuffer(ShadowRes, ShadowRes, 0, true, true);
    gBuffers = Framebuffer(gWidth, gHeight, gBufferCount, true, false);
    dBuffer = Framebuffer(gWidth, gHeight, 0, true, false);

    // Set constant uniforms
    shaders[UIShader].bind();
    shaders[UIShader].setUniformI("u_texture_array", 0);
    shaders[UIShader].setUniform("u_buffer_width", float(WindowWidth));
    shaders[UIShader].setUniform("u_buffer_height", float(WindowHeight));

    shaders[DefaultShader].bind();
    shaders[DefaultShader].setUniformI("u_diffuse", 0);

    shaders[OpqaueShader].bind();
    shaders[OpqaueShader].setUniformI("u_diffuse", 0);

    shaders[TranslucentShader].bind();
    shaders[TranslucentShader].setUniformI("u_diffuse", 0);

    float fisheyeFactor = 0.8f;
    shaders[FinalShader].bind();
    shaders[FinalShader].setUniformI("u_diffuse_buffer", 0);
    shaders[FinalShader].setUniformI("u_normal_buffer", 1);
    shaders[FinalShader].setUniformI("u_material_buffer", 2);
    shaders[FinalShader].setUniformI("u_depth_buffer", gBufferCount + 0);
    shaders[FinalShader].setUniformI("u_shadow_texture", gBufferCount + 1);
    shaders[FinalShader].setUniformI("u_noise_texture", gBufferCount + 2);
    shaders[FinalShader].setUniform("u_buffer_width", static_cast<float>(gWidth));
    shaders[FinalShader].setUniform("u_buffer_height", static_cast<float>(gHeight));
    shaders[FinalShader].setUniform("u_shadow_texture_resolution", static_cast<float>(ShadowRes));
    shaders[FinalShader].setUniform("u_shadow_fisheye_factor", fisheyeFactor);
    shaders[FinalShader].setUniform("u_shadow_distance", static_cast<float>(getShadowDistance() * 16));
    shaders[FinalShader].setUniform("u_render_distance", static_cast<float>(RenderDistance * 16));

    shaders[ShadowShader].bind();
    shaders[ShadowShader].setUniformI("u_diffuse", 0);
    shaders[ShadowShader].setUniform("u_shadow_fisheye_factor", fisheyeFactor);

    Shader::unbind();
}

Mat4f getShadowMatrix() {
    auto length = static_cast<float>(getShadowDistance() * 16);
    auto res = Mat4f(1.0f);
    res = Mat4f::rotate(-static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f)) * res;
    res = Mat4f::rotate(static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)) * res;
    res = Mat4f::ortho(-length, length, -length, length, -1000.0f, 1000.0f) * res;
    return res;
}

Mat4f getShadowMatrixExperimental(float fov, float aspect, double heading, double pitch) {
    auto length = static_cast<float>(getShadowDistance() * 16);
    auto res = Mat4f(1.0f);
    res = Mat4f::rotate(-static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f)) * res;
    res = Mat4f::rotate(static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)) * res;

    // Calculate view direction in light space, then rotate it to right (+1, 0)
    auto viewRotate = Mat4f(1.0f);
    viewRotate *= Mat4f::rotate(static_cast<float>(heading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f));
    viewRotate *= Mat4f::rotate(-static_cast<float>(pitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f));
    auto viewDir = (res * viewRotate).transform(Vec3f(0.0f, 0.0f, -1.0f));
    auto viewDirXY = Vec3f(viewDir.x, viewDir.y, 0.0f);
    if (viewDirXY.length() > 0.01f) {
        float radians = std::atan2(viewDir.y, viewDir.x);
        res = Mat4f::rotate(-radians, Vec3f(0.0f, 0.0f, 1.0f)) * res;
    }

    // Minimal bounding box containing a diamond-shaped convex hull
    // (should approximate the visible parts better than the whole view frustum)
    float halfHeight = std::tan(static_cast<float>(fov * Pi / 180.0) / 2.0f);
    float halfWidth = halfHeight * static_cast<float>(aspect);
    auto vertices = std::array{
        Vec3f(0.0f, 0.0f, -1.0f),
        Vec3f(-halfWidth, -halfHeight, -1.0f),
        Vec3f(halfWidth, -halfHeight, -1.0f),
        Vec3f(halfWidth, halfHeight, -1.0f),
        Vec3f(-halfWidth, halfHeight, -1.0f),
    };
    auto toLightSpace = res * viewRotate;
    float xmin = 0.0f, xmax = 0.0f, ymin = 0.0f, ymax = 0.0f;
    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].normalize();
        vertices[i] *= length;
        vertices[i] = toLightSpace.transform(vertices[i]);
        xmin = std::min(xmin, vertices[i].x);
        xmax = std::max(xmax, vertices[i].x);
        ymin = std::min(ymin, vertices[i].y);
        ymax = std::max(ymax, vertices[i].y);
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

void StartShadowPass(Mat4f const& shadowMatrix, float gameTime) {
    assert(AdvancedRender);

    // Bind output target buffers
    shadow.bindTargets();

    // Set dynamic uniforms
    Shader& shader = shaders[ShadowShader];
    bindShader(ShadowShader);
    shader.setUniform("u_mvp", shadowMatrix);
    shader.setUniform("u_game_time", gameTime);

    // Disable unwanted defaults
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

void EndShadowPass() {
    assert(AdvancedRender);

    // Unbind output target buffers
    shadow.unbindTarget();

    // Disable shader
    Shader::unbind();

    // Enable defaults
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void StartOpaquePass(Mat4f const& viewMatrix, float gameTime) {
    // Bind output target buffers
    if (AdvancedRender)
        gBuffers.bindTargets();

    // Set dynamic uniforms
    Shader& shader = shaders[AdvancedRender ? OpqaueShader : DefaultShader];
    bindShader(AdvancedRender ? OpqaueShader : DefaultShader);
    shader.setUniform("u_mvp", viewMatrix);
    shader.setUniform("u_game_time", gameTime);

    // Disable unwanted defaults
    glDisable(GL_BLEND);
}

void EndOpaquePass() {
    // Unbind output target buffers
    if (AdvancedRender)
        gBuffers.unbindTarget();

    // Disable shader
    Shader::unbind();

    // Enable defaults
    glEnable(GL_BLEND);
}

void StartTranslucentPass(Mat4f const& viewMatrix, float gameTime) {
    // Copy the depth component of the G-buffer to the D-buffer, bind output target buffers
    if (AdvancedRender) {
        // gBuffers.copyDepthTexture(dBuffer);
        gBuffers.bindTargets();
    }

    // Set dynamic uniforms
    Shader& shader = shaders[AdvancedRender ? TranslucentShader : DefaultShader];
    bindShader(AdvancedRender ? TranslucentShader : DefaultShader);
    shader.setUniform("u_mvp", viewMatrix);
    shader.setUniform("u_game_time", gameTime);
}

void EndTranslucentPass() {
    // Unbind output target buffers
    if (AdvancedRender)
        gBuffers.unbindTarget();

    // Disable shader
    Shader::unbind();
}

void StartFinalPass(
    double xpos,
    double ypos,
    double zpos,
    Mat4f const& viewMatrix,
    Mat4f const& shadowMatrix,
    float gameTime
) {
    assert(AdvancedRender);

    // Bind textures to pre-defined slots
    gBuffers.bindColorTextures(0);
    gBuffers.bindDepthTexture(gBufferCount + 0);
    shadow.bindDepthTexture(gBufferCount + 1);
    glActiveTexture(GL_TEXTURE0 + gBufferCount + 2);
    glBindTexture(GL_TEXTURE_2D, getNoiseTexture());
    glActiveTexture(GL_TEXTURE0);

    // Set dynamic uniforms
    int repeat = 25600;
    int ixpos = int(floor(xpos)), iypos = int(floor(ypos)), izpos = int(floor(zpos));
    auto sunlightDir = (Mat4f::rotate(static_cast<float>(sunlightHeading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f))
                        * Mat4f::rotate(-static_cast<float>(sunlightPitch * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)))
                           .transform(Vec3f(0.0f, 0.0f, -1.0f));

    Shader& shader = shaders[FinalShader];
    bindShader(FinalShader);
    shader.setUniform("u_mvp", viewMatrix);
    shader.setUniform("u_shadow_mvp", shadowMatrix);
    shader.setUniform("u_sunlight_dir", sunlightDir);
    shader.setUniform("u_game_time", gameTime);
    shader.setUniformI("u_repeat_length", repeat);
    shader.setUniformI("u_player_coord_int", ixpos, iypos, izpos);
    shader.setUniformI("u_player_coord_mod", ixpos % repeat, iypos % repeat, izpos % repeat);
    shader.setUniform("u_player_coord_frac", float(xpos - ixpos), float(ypos - iypos), float(zpos - izpos));
}

void EndFinalPass() {
    assert(AdvancedRender);

    Shader::unbind();
}

VertexBuffer VertexBuffer::upload(bool staticDraw) {
    VertexBuffer res;
    res.primitive = Primitive;
    res.numVertices = Vertexes;

    int vc = Coordc, tc = Texcoordc, cc = Colorc, nc = Normalc, ac = Attribc;
    int cnt = vc + tc + cc + nc + ac;

    glGenVertexArrays(1, &res.vao);
    glBindVertexArray(res.vao);

    glGenBuffers(1, &res.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        Vertexes * (cnt * sizeof(float)),
        VertexArray,
        staticDraw ? GL_STATIC_DRAW : GL_STREAM_DRAW
    );

    GLuint arrays = 0;
    if (vc > 0) {
        glEnableVertexAttribArray(arrays);
        glVertexAttribPointer(arrays++, vc, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*) (0 * sizeof(float)));
    }
    if (tc > 0) {
        glEnableVertexAttribArray(arrays);
        glVertexAttribPointer(arrays++, tc, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*) (vc * sizeof(float)));
    }
    if (cc > 0) {
        glEnableVertexAttribArray(arrays);
        glVertexAttribPointer(
            arrays++,
            cc,
            GL_FLOAT,
            GL_FALSE,
            cnt * sizeof(float),
            (float*) ((vc + tc) * sizeof(float))
        );
    }
    if (nc > 0) {
        glEnableVertexAttribArray(arrays);
        glVertexAttribPointer(
            arrays++,
            nc,
            GL_FLOAT,
            GL_FALSE,
            cnt * sizeof(float),
            (float*) ((vc + tc + cc) * sizeof(float))
        );
    }
    if (ac > 0) {
        glEnableVertexAttribArray(arrays);
        glVertexAttribPointer(
            arrays++,
            ac,
            GL_FLOAT,
            GL_FALSE,
            cnt * sizeof(float),
            (float*) ((vc + tc + cc + nc) * sizeof(float))
        );
    }
    return res;
}

void VertexBuffer::render() const {
    glBindVertexArray(vao);
    glDrawArrays(primitive, 0, numVertices);
}
}
