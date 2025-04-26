#include "VertexAllocator.h"

namespace Renderer {
  VertexPool::VertexPool(GLuint capacity): capacity(capacity), free_list(capacity) {
    free_head = 0;
    for (GLuint i = 0; i < capacity; i++) free_list[i] = i + 1;

    glGenBuffers(1, &array_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, array_buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, capacity * sizeof(float), nullptr, 0);
  }
}
