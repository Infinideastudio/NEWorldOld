module;

#include <glad/gl.h>

export module render:block_builder;
import std;
import types;
import debug;
import math;
import :types;
import :block_layout;

namespace render {
using types::FixedString;
using namespace block_layout;

// An interface block on the CPU side.
export template <Layout T>
class Block {
public:
    // `Field<name>` gives the type of the field.
    template <FixedString name>
    using Field = typename decltype(locate_v<T, name>)::cpp_type;

    Block() = default;

    template <typename Self>
    auto bytes(this Self&& self) -> auto&& {
        return std::forward<Self>(self)._bytes;
    }

    template <FixedString name>
    auto get() const -> Field<name> {
        constexpr auto located = locate_v<T, name>;
        static_assert(sizeof(Field<name>) == located.size);
        auto attr = Field<name>();
        auto span = std::as_writable_bytes(std::span<Field<name>, 1>(&attr, 1));
        auto src = _bytes.begin() + located.offset;
        std::copy_n(src, span.size(), span.begin());
        return attr;
    }

    template <FixedString name>
    void set(Field<name> attr) {
        constexpr auto located = locate_v<T, name>;
        static_assert(sizeof(Field<name>) == located.size);
        auto span = std::as_bytes(std::span<Field<name>, 1>(&attr, 1));
        auto dst = _bytes.begin() + located.offset;
        std::copy_n(span.begin(), span.size(), dst);
    }

private:
    std::array<std::byte, T::size> _bytes;
};

}
