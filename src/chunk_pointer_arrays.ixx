module;

#include <algorithm>
#include <memory>

export module chunk_pointer_arrays;
import vec3;
import chunks;

export class ChunkPtrArray {
public:
    explicit ChunkPtrArray(std::size_t size):
        size(size),
        array(std::make_unique<Chunk*[]>(size * size * size)) {
        std::fill(array.get(), array.get() + size * size * size, nullptr);
    }

    auto getSize() -> std::size_t {
        return size;
    }

    auto getOrigin() -> Vec3i {
        return origin;
    }

    void move(Vec3i offset) {
        auto arrTemp = std::make_unique<Chunk*[]>(size * size * size);
        for (int x = 0; x < size; x++) {
            for (int y = 0; y < size; y++) {
                for (int z = 0; z < size; z++) {
                    auto v = Vec3i(x, y, z) + offset;
                    if (elementExists(v))
                        arrTemp[(x * size + y) * size + z] = array[(v.x * size + v.y) * size + v.z];
                    else
                        arrTemp[(x * size + y) * size + z] = nullptr;
                }
            }
        }
        array = std::move(arrTemp);
        origin += offset;
    }

    void moveTo(Vec3i coord) {
        if (coord != origin)
            move(coord - origin);
    }

    auto elementExists(Vec3i v) -> bool {
        return v.x >= 0 && v.x < size && v.y >= 0 && v.y < size && v.z >= 0 && v.z < size;
    }

    auto getChunkPtr(Vec3i coord) -> Chunk* {
        auto v = coord - origin;
        if (elementExists(v))
            return array[(v.x * size + v.y) * size + v.z];
        return nullptr;
    }

    void setChunkPtr(Vec3i coord, Chunk* c) {
        auto v = coord - origin;
        if (elementExists(v))
            array[(v.x * size + v.y) * size + v.z] = c;
    }

private:
    std::size_t size = 0;
    std::unique_ptr<Chunk*[]> array;
    Vec3i origin = Vec3i(0, 0, 0);
};
