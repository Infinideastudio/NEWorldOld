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
};

template <template <typename> typename W, typename T>
struct wrapped<W, W<T>> {
    static constexpr auto value = true;
};

// Returns the index of the first element in the argument list `T` that is wrapped by
// a template `W`, or `N` if none is found.
template <template <typename> typename W, typename... T>
struct wrapped_index {
    static constexpr auto value = true_index(std::array{wrapped<W, T>::value...});
};

template <template <typename> typename W, typename... T>
constexpr auto wrapped_index_v = wrapped_index<W, T...>::value;

// Universal wrapper for uniquely-owned resources with stateless deleters.
template <typename T, T empty, typename F, F deleter>
requires std::invocable<F, T>
class Resource {
public:
    // Constructs a `Resource` which owns nothing.
    Resource():
        _handle(empty) {}

    // Constructs a `Resource` which owns the given `handle`.
    explicit Resource(T handle):
        _handle(handle) {}

    // Replaces the managed object.
    void reset(T handle) noexcept {
        this->~Resource();
        _handle = handle;
    }

    Resource(Resource const&) = delete;
    Resource(Resource&& from) noexcept:
        _handle(std::exchange(from._handle, empty)) {}

    auto operator=(Resource const&) -> Resource& = delete;
    auto operator=(Resource&& from) noexcept -> Resource& {
        this->~Resource();
        _handle = std::exchange(from._handle, empty);
        return *this;
    }

    // Destroys the managed object if it owns one.
    ~Resource() {
        if (_handle != empty) {
            std::invoke(deleter, _handle);
        }
    }

    // Returns whether it owns a managed object.
    // Marked explicit to avoid accidental conversions (as `bool` might get promoted into `T`).
    explicit operator bool() const noexcept {
        return _handle != empty;
    }

    // Returns the underlying handle to the managed object.
    // The handle is `empty` if it currently owns nothing.
    auto get() const noexcept -> T {
        return _handle;
    }

    // Releases and returns the underlying handle.
    // The resource is no longer owned and will not be automatically deleted.
    auto release() noexcept -> T {
        return std::exchange(_handle, empty);
    }

private:
    T _handle;
};

}
