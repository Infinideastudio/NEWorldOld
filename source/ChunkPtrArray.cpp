#include "chunkPtrArray.h"

namespace World {
    
    void ChunkPtrArray::setSize(int s){
        size = s;
        size2 = size*size;
        size3 = size*size*size;
    }

    bool ChunkPtrArray::create(){
        array = new Chunk*[size3];
        if (array == nullptr) return false;
        memset(array, (int)nullptr, size3*sizeof(Chunk*));
        return true;
    }

    void ChunkPtrArray::destroy(){
        delete[] array;
        array = nullptr;
    }

    void ChunkPtrArray::move(int xd, int yd, int zd){
        Chunk** arrTemp = new Chunk*[size3];
        for (int x = 0; x < size; x++) {
            for (int y = 0; y < size; y++) {
                for (int z = 0; z < size; z++) {
                    if (elementExists(x + xd, y + yd, z + zd))arrTemp[x*size2 + y*size + z] = array[(x + xd)*size2 + (y + yd)*size + (z + zd)];
                    else arrTemp[x*size2 + y*size + z] = nullptr;
                }
            }
        }
        delete[] array;
        array = arrTemp;
        originX += xd; originY += yd; originZ += zd;
    }

    void ChunkPtrArray::moveTo(int x, int y, int z) {
        move(x - originX, y - originY, z - originZ);
    }

    Chunk* ChunkPtrArray::getChunkPtr(int x, int y, int z) {
        x -= originX; y -= originY; z -= originZ;
        if (!elementExists(x, y, z)) return nullptr;
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
        c_getChunkPtrFromCPA++;
#endif
        return array[x*size2 + y*size + z];
    }

    void ChunkPtrArray::setChunkPtr(int x, int y, int z, Chunk* c) {
        x -= originX; y -= originY; z -= originZ;
        if (elementExists(x, y, z)) array[x*size2 + y*size + z] = c;
    }
}
