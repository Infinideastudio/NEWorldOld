export module render:types;
import std;
import types;

// Some metaprogramming helpers.
// This submodule is for use within the `render` module only, and does not export symbols.
namespace render {

// Returns the sum of all elements in the array.
template <size_t N>
consteval auto sum(std::array<size_t, N> a) -> size_t {
    return std::accumulate(a.begin(), a.end(), 0);
}

// Returns the prefix sum of all elements in the array.
template <size_t N>
consteval auto prefix_sum(std::array<size_t, N> a) -> std::array<size_t, N> {
    auto res = std::array<size_t, N>{};
    auto sum = 0uz;
    for (auto i = 0uz; i < N; i++) {
        res[i] = sum;
        sum += a[i];
    }
    return res;
}

// Converts a string to an integer at compile time.
consteval auto stouz(std::string_view s) -> size_t {
    auto index = 0uz;
    for (char i: s) {
        index *= 10;
        index += i - '0';
    }
    return index;
}

// Returns the index of the first `true` element in the array, or `N` if none is found.
template <size_t N>
consteval auto true_index(std::array<bool, N> const& a) {
    return std::find(a.begin(), a.end(), true) - a.begin();
}

// Returns whether the type `T` is wrapped by the template `W`, and the wrapped type.
template <template <typename> typename W, typename T>
struct wrapped {
    static constexpr auto value = false;
    using wrapped_type = std::monostate;
};

template <template <typename> typename W, typename T>
struct wrapped<W, W<T>> {
    static constexpr auto value = true;
    using wrapped_type = T;
};

// The implementation of `wrapped_index`.
template <template <typename> typename W, typename... T>
consteval auto _wrapped_index() -> size_t {
    auto wrapped_matches = std::array{wrapped<W, T>::value...};
    return true_index(wrapped_matches);
}

// Returns the index of the first element in the argument list `T` that is wrapped by
// a template `W`, or `N` if none is found.
template <template <typename> typename W, typename... T>
struct wrapped_index {
    static constexpr auto value = _wrapped_index<W, T...>();
};

template <template <typename> typename W, typename... T>
constexpr auto wrapped_index_v = wrapped_index<W, T...>::value;

}
