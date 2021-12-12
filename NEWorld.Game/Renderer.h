#pragma once

#include "Definitions.h"
#include "Shader.h"
#include <cstring>

namespace Renderer {
    //我猜你肯定不敢看Renderer.cpp  --qiaozhanrong
    //猜对了  --Null

    enum {
        MainShader, MergeFaceShader, ShadowShader, DepthShader
    };

    const int ArraySize = 2621440;
    extern float *VA;
    extern int size;
    extern int Vertexes;
    extern bool AdvancedRender;
    extern int ShadowRes;
    extern int MaxShadowDist;
    extern int shadowdist;
    extern float sunlightXrot, sunlightYrot;
    extern unsigned int DepthTexture;
    extern std::vector<Shader> shaders;
    extern int ActiveShader;

    void Init(int tc, int cc, int ac = 0);

    void Vertex3f(float x, float y, float z);

    void TexCoord2f(float x, float y);

    void TexCoord3f(float x, float y, float z);

    void Color3f(float r, float g, float b);

    void Color4f(float r, float g, float b, float a);

    void Attrib1f(float attr);

    inline void Vertex3d(double x, double y, double z) { Vertex3f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }

    inline void TexCoord2d(double x, double y) { TexCoord2f(static_cast<float>(x), static_cast<float>(y)); }

    inline void TexCoord3d(double x, double y, double z) { TexCoord3f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }

    inline void Color3d(double r, double g, double b) { Color3f(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)); }

    inline void Color4d(double r, double g, double b, double a) { Color4f(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)); }

    inline void Quad(float *geomentry) {
        //这样做貌似提升不了性能吧。。。 --qiaozhanrong
        memcpy(VA, geomentry, size * sizeof(float));
        VA += size;
        Vertexes += 4;
    }

    void Flush(VBOID &buffer, vtxCount &vtxs);

    void BatchStart(int tc, int cc, int ac) noexcept;

    void RenderBuffer(VBOID buffer, vtxCount vtxs);

    void RenderBufferDirect(VBOID buffer, vtxCount vtxs, int tc, int cc, int ac = 0);

    void initShaders();

    inline void bindShader(int shaderID) {
        shaders[shaderID].bind();
        ActiveShader = shaderID;
    }

    void destroyShaders();

    void EnableShaders();

    void DisableShaders();

    void StartShadowPass();

    void EndShadowPass();
}