module;

#include <algorithm>
#include <memory>

export module height_maps;
import vec3;
import terrain_generation;

export class HeightMap {
public:
    static constexpr int WATER_LEVEL = WorldGen::WaterLevel;

    explicit HeightMap(std::size_t size):
        _size(size),
        _array(std::make_unique<int[]>(size * size)) {
        std::fill(_array.get(), _array.get() + size * size, -1);
    }

    auto sSize() -> std::size_t {
        return _size;
    }

    auto origin() -> Vec3i {
        return _origin;
    }

    void move(Vec3i offset) {
        auto next = std::make_unique<int[]>(_size * _size);
        for (int x = 0; x < _size; x++) {
            for (int z = 0; z < _size; z++) {
                auto v = Vec3i(x, 0, z) + offset;
                if (contains(v))
                    next[x * _size + z] = _array[v.x * _size + v.z];
                else
                    next[x * _size + z] = -1;
            }
        }
        _array = std::move(next);
        _origin += offset;
    }

    void set_center(Vec3i coord) {
        if (coord != _origin)
            move(coord - _origin);
    }

    auto contains(Vec3i v) -> bool {
        return v.x >= 0 && v.x < _size && v.z >= 0 && v.z < _size;
    }

    auto get(Vec3i coord) -> int {
        auto v = coord - _origin;
        if (contains(v)) {
            if (_array[v.x * _size + v.z] == -1)
                _array[v.x * _size + v.z] = WorldGen::getHeight(coord.x, coord.z);
            return _array[v.x * _size + v.z];
        }
        return WorldGen::getHeight(coord.x, coord.z);
    }

private:
    std::size_t _size = 0;
    std::unique_ptr<int[]> _array;
    Vec3i _origin = Vec3i(0, 0, 0);
};
