module;

#include <algorithm>
#include <memory>

export module height_maps;
import vec3;
import terrain_generation;

export class HeightMap {
public:
    static constexpr int WaterLevel = WorldGen::WaterLevel;
  
    explicit HeightMap(std::size_t size):
        size(size),
        array(std::make_unique<int[]>(size * size)) {
        std::fill(array.get(), array.get() + size * size, -1);
    }

    auto getSize() -> std::size_t {
        return size;
    }

    auto getOrigin() -> Vec3i {
        return origin;
    }

    void move(Vec3i offset) {
        auto arrTemp = std::make_unique<int[]>(size * size);
        for (int x = 0; x < size; x++) {
            for (int z = 0; z < size; z++) {
                auto v = Vec3i(x, 0, z) + offset;
                if (elementExists(v))
                    arrTemp[x * size + z] = array[v.x * size + v.z];
                else
                    arrTemp[x * size + z] = -1;
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
        return v.x >= 0 && v.x < size && v.z >= 0 && v.z < size;
    }

    auto getHeight(Vec3i coord) -> int {
        auto v = coord - origin;
        if (elementExists(v)) {
            if (array[v.x * size + v.z] == -1)
                array[v.x * size + v.z] = WorldGen::getHeight(coord.x, coord.z);
            return array[v.x * size + v.z];
        }
        return WorldGen::getHeight(coord.x, coord.z);
    }

private:
    std::size_t size = 0;
    std::unique_ptr<int[]> array;
    Vec3i origin = Vec3i(0, 0, 0);
};
