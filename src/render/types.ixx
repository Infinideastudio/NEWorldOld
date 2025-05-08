export module render:types;
import std;
import types;

// Some metaprogramming helpers.
// This submodule is for use within the `render` module only, and does not export symbols.
namespace render {

// Returns the sum of all elements in the array.
template <size_t N>
constexpr auto sum(std::array<size_t, N> a) -> size_t {
    return std::accumulate(a.begin(), a.end(), 0);
}

// Returns the prefix sum of all elements in the array.
template <size_t N>
constexpr auto prefix_sum(std::array<size_t, N> a) -> std::array<size_t, N> {
    auto res = std::array<size_t, N>{};
    auto sum = 0uz;
    for (auto i = 0uz; i < N; i++) {
        res[i] = sum;
        sum += a[i];
    }
    return res;
}

// The index denoting "not found".
constexpr auto not_found = static_cast<size_t>(-1);

// The result type of `find_wrapper()`.
template <size_t I, typename T>
struct find_wrapper_result {
    // The index of the first element in argument list `A` that is wrapped by a template `T`.
    static constexpr auto index = I;
    // Whether the wrapper was found.
    static constexpr auto found = (I != not_found);
    // The type wrapped by `T`.
    using type = T;
};

// Finds the first element in the argument list `A` that is wrapped by a template `T`.
template <typename T, typename A, size_t I = 0>
requires types::is_temp_v<T> && types::is_args_v<A>
constexpr auto find_wrapper() {
    if constexpr (A::empty) {
        // If the argument list is empty, return "not found".
        return find_wrapper_result<not_found, std::monostate>{};
    } else {
        // Otherwise, check the head element.
        using head = typename A::head;
        using tail = typename A::tail;
        if constexpr (A::head::atomic) {
            // If the head element is atomic, it cannot be `T`, so we continue searching.
            return find_wrapper<T, tail, I + 1>();
        } else {
            // If the head element is a template, check if it is `T`.
            if constexpr (types::temp_is_same_v<typename head::temp, T>) {
                // If it is, return the index and the wrapped type.
                return find_wrapper_result<I, typename head::args::head::get>{};
            } else {
                // Otherwise, continue searching.
                return find_wrapper<T, tail, I + 1>();
            }
        }
    }
}

template <template <typename> typename T, typename... A>
using find_wrapper_t = decltype(find_wrapper<types::temp<T>, types::args<A...>>());

}
