#pragma once

#include <GL/glew.h>

class ChunkVertexHolder {
public:
    ChunkVertexHolder(int size);

    ~ChunkVertexHolder() noexcept;

    void* MapWriteOnly();

    void Unmap();

private:
    GLuint mHandle;
};
