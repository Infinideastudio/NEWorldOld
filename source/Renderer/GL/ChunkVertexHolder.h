#pragma once

#include <GL/glew.h>
#include "Renderer/World/ChunkVertexBuilder.h"

class ChunkVertexHolder {
public:
    explicit ChunkVertexHolder(int size);

    ~ChunkVertexHolder() noexcept;

    void *MapWriteOnly();

    void Unmap();

    void Bind() noexcept;

private:
    GLuint mHandle{};
};

class ChunkVertexRenderer {
public:
    explicit ChunkVertexRenderer(const ChunkVertexBuilder &builder);

private:
    inline static float gTexturePerLine;
    ChunkVertexHolder mHolder;
};
