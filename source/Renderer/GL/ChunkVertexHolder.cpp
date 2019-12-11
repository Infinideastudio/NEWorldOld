//
// Created by User on 2019/12/11.
//

#include "ChunkVertexHolder.h"

ChunkVertexHolder::ChunkVertexHolder(int size) {
    glGenBuffers(1, &mHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mHandle);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STATIC_DRAW);
}

ChunkVertexHolder::~ChunkVertexHolder() noexcept {
    glDeleteBuffers(1, &mHandle);
}

void *ChunkVertexHolder::MapWriteOnly() {
    glBindBuffer(GL_ARRAY_BUFFER, mHandle);
    return glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
}

void ChunkVertexHolder::Unmap() {
    glBindBuffer(GL_ARRAY_BUFFER, mHandle);
    glUnmapBuffer(GL_ARRAY_BUFFER);
}
