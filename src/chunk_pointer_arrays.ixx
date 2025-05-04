export module chunk_pointer_arrays;
import std;
import types;
import vec3;
import chunks;

export class ChunkPointerArray {
public:
    explicit ChunkPointerArray(std::size_t size):
        _size(size),
        _array(std::make_unique<chunks::Chunk*[]>(size * size * size)) {
        std::fill(_array.get(), _array.get() + size * size * size, nullptr);
    }

    auto size() const -> std::size_t {
        return _size;
    }

    auto origin() const -> Vec3i {
        return _origin;
    }

    void move(Vec3i offset) {
        auto next = std::make_unique<chunks::Chunk*[]>(_size * _size * _size);
        for (int x = 0; x < _size; x++) {
            for (int y = 0; y < _size; y++) {
                for (int z = 0; z < _size; z++) {
                    auto v = Vec3i(x, y, z) + offset;
                    if (contains(v))
                        next[(x * _size + y) * _size + z] = _array[(v.x * _size + v.y) * _size + v.z];
                    else
                        next[(x * _size + y) * _size + z] = nullptr;
                }
            }
        }
        _array = std::move(next);
        _origin += offset;
    }

    void set_center(Vec3i coord) {
        if (coord != _origin)
            move(coord - _origin);
    }

    auto contains(Vec3i v) const -> bool {
        return v.x >= 0 && v.x < _size && v.y >= 0 && v.y < _size && v.z >= 0 && v.z < _size;
    }

    auto get(Vec3i coord) const -> chunks::Chunk* {
        auto v = coord - _origin;
        if (contains(v))
            return _array[(v.x * _size + v.y) * _size + v.z];
        return nullptr;
    }

    void set(Vec3i coord, chunks::Chunk* c) {
        auto v = coord - _origin;
        if (contains(v))
            _array[(v.x * _size + v.y) * _size + v.z] = c;
    }

private:
    std::size_t _size = 0;
    std::unique_ptr<chunks::Chunk*[]> _array;
    Vec3i _origin = Vec3i(0, 0, 0);
};
