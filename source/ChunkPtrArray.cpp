#include "ChunkPtrArray.h"
#include <cstring>

namespace World {
	void ChunkPtrArray::Create(int s){
        size = s;
        size2 = size*size;
        size3 = size*size*size;
        man = {size2, size, 1};
		array = new Chunk*[size3];
		memset(array, 0, size3*sizeof(Chunk*));
	}

	void ChunkPtrArray::Finalize(){
		delete[] array;
		array = nullptr;
	}

	void ChunkPtrArray::Move(const Int3& delta){
	    if (delta.X || delta.Y || delta.Z) {
	        Int3 iter {};
            auto** arrTemp = new Chunk* [size3];
            for (iter.X = 0; iter.X<size; iter.X++) {
                for (iter.Y = 0; iter.Y<size; iter.Y++) {
                    for (iter.Z = 0; iter.Z<size; iter.Z++) {
                        arrTemp[Dot(iter, man)] = Fetch(iter + delta);
                    }
                }
            }
            delete[] array;
            array = arrTemp;
            origin += delta;
        }
	}
}