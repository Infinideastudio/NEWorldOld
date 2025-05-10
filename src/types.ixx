export module types;
import std;

export using std::int8_t;
export using std::int16_t;
export using std::int32_t;
export using std::int64_t;
export using std::ptrdiff_t;
export using std::uint8_t;
export using std::uint16_t;
export using std::uint32_t;
export using std::uint64_t;
export using std::size_t;

// # Some metaprogramming facilities.
//
// The structure of C++ type template parameters can be defined by the following datatype:
//
// ```haskell
// data Type = Atomic Name
//           | Template Name Args
//
// data Args = Nil
//           | Cons Type Args
// ```
//
// To enable the use (consumption, elimination) of types in `constexpr` programming, we need
// some pattern-matching primitives on this structure. Then, we should be possible to compute with
// any generic type using `constexpr` functions, except that sometimes we still need standard
// library intrisics (so-called "type traits") to inspect atomic names.
//
// Still, we cannot deal with **non-type** template parameters like integers, which may be present
// in templates like e.g. `std::array<T, N>`. These would still require special handling.
//
// (终究有一天 qzr 变成了曾经讨厌的样子？)
namespace types {

/*
export template <typename... T>
struct args;

// The atomic name wrapper.
export template <typename T>
struct atom {
    // The actual atomic type.
    using get = T;
    // Injection to value level.
    constexpr atom() = default;
};

// The template name wrapper.
export template <template <typename...> typename T>
struct temp {
    // The actual template.
    template <typename... U>
    using get = T<U...>;
    // Injection to value level.
    constexpr temp() = default;
};

// The type inspector: atomic case.
export template <typename T>
struct type {
    // Indicates whether the template argument is an atomic type.
    static constexpr auto atomic = true;
    // The actual type.
    using get = T;
    // Obtains the atomic type name.
    using atom = atom<T>;
    // Injection to value level.
    constexpr type() = default;
};

// The type inspector: template case.
export template <template <typename...> typename T, typename... U>
struct type<T<U...>> {
    // Indicates whether the template argument is an atomic type.
    static constexpr auto atomic = false;
    // The actual type.
    using get = T<U...>;
    // Obtains the template type name.
    using temp = temp<T>;
    // Obtains the argument list.
    using args = args<U...>;
    // Injection to value level.
    constexpr type() = default;
};

// The argument list inspector: nil case.
template <typename... T>
struct args {
    // Indicates whether the argument list is empty.
    static constexpr auto empty = true;
    // Compiler intrinsic for the length of the argument list.
    static constexpr auto length = 0uz;
    // Injection to value level.
    constexpr args() = default;
};

// The argument list inspector: cons case.
template <typename T, typename... U>
struct args<T, U...> {
    // Indicates whether the argument list is empty.
    static constexpr auto empty = false;
    // Obtains the first element of the argument list.
    using head = type<T>;
    // Obtains the remaining elements of the argument list.
    using tail = args<U...>;
    // Compiler intrinsic for the length of the argument list.
    static constexpr auto length = 1uz + sizeof...(U);
    // Injection to value level.
    constexpr args() = default;
};

// Structural construstors of inspectors.
template <typename... T>
struct make_type {};
template <typename... T>
struct make_args {};

// The type inspector: atomic case.
template <typename T>
struct make_type<atom<T>> {
    using type = type<T>;
};

// The type inspector: template case.
template <template <typename...> typename T, typename... U>
struct make_type<temp<T>, args<U...>> {
    using type = type<T<U...>>;
};

// The argument list inspector: nil case.
template <>
struct make_args<> {
    using type = args<>;
};

// The argument list inspector: cons case.
template <typename T, typename... U>
struct make_args<type<T>, args<U...>> {
    using type = args<T, U...>;
};

export template <typename... T>
using make_type_t = typename make_type<T...>::type;
export template <typename... T>
using make_args_t = typename make_args<T...>::type;

// Type-checking constants.
// For use in `requires` clauses to provide sane error messages.
export template <typename T>
constexpr auto is_atom_v = false;
template <typename T>
constexpr auto is_atom_v<atom<T>> = true;

export template <typename T>
constexpr auto is_temp_v = false;
template <template <typename...> typename T>
constexpr auto is_temp_v<temp<T>> = true;

export template <typename T>
constexpr auto is_type_v = false;
template <typename T>
constexpr auto is_type_v<type<T>> = true;

export template <typename T>
constexpr auto is_args_v = false;
template <typename... T>
constexpr auto is_args_v<args<T...>> = true;

// Returns if two atomic types are equal.
export template <typename T, typename U>
constexpr auto atom_is_same_v = false;
template <typename T>
constexpr auto atom_is_same_v<atom<T>, atom<T>> = true;

// Returns if two template types are equal.
export template <typename T, typename U>
constexpr auto temp_is_same_v = false;
template <template <typename...> typename T>
constexpr auto temp_is_same_v<temp<T>, temp<T>> = true;

// Returns if two types are structurally equal.
export template <typename T, typename U>
constexpr auto type_is_same_v = false;
template <typename T>
constexpr auto type_is_same_v<type<T>, type<T>> = true;

// Returns if two argument lists are structurally equal.
export template <typename T, typename U>
constexpr auto args_is_same_v = false;
template <typename... T>
constexpr auto args_is_same_v<args<T...>, args<T...>> = true;

// Returns the i-th element in the argument list `A`. Switch to pack indexing once C++26 is available.
export template <size_t I, typename A>
requires is_args_v<A>
constexpr auto elem() {
    if constexpr (A::empty) {
        static_assert(false, "index out of bound");
    } else if constexpr (I == 0) {
        return typename A::head{};
    } else {
        return elem<I - 1, typename A::tail>();
    }
}

export template <size_t I, typename A>
requires is_args_v<A>
using elem_t = decltype(elem<I, A>())::get;

// Some compile-time unit tests.
static_assert(std::is_same_v<elem_t<0, args<int8_t, int16_t, int32_t, int64_t>>, int8_t>);
static_assert(std::is_same_v<elem_t<2, args<int8_t, int16_t, int32_t, int64_t>>, int32_t>);
*/

}
