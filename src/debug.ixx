export module debug;
import std;

// "Unreachable" mark.
export [[noreturn]] void unreachable(std::source_location const& loc = std::source_location::current()) {
    std::println(std::cerr, "{}: In function '{}':", loc.file_name(), loc.function_name());
    std::println(std::cerr, "{}:{}:{}: \"unreachable\" code was reached", loc.file_name(), loc.line(), loc.column());
    std::terminate();
}

// "Unimplemented" mark.
export [[noreturn]] void unimplemented(std::source_location const& loc = std::source_location::current()) {
    std::println(std::cerr, "{}: In function '{}':", loc.file_name(), loc.function_name());
    std::println(std::cerr, "{}:{}:{}: \"unimplemented\" code was called", loc.file_name(), loc.line(), loc.column());
    std::terminate();
}

// Assertion. No effect under release mode.
// Note that (semantically) `expr` is evaluated even in release mode, which is different from using
// a macro, so any side effect of `expr` will remain. If there is no side effect, the compiler can
// optimize it out.
export void
assert(bool expr, std::string_view msg = "", std::source_location const& loc = std::source_location::current()) {
    // #ifndef NDEBUG
    if (!expr) {
        std::println(std::cerr, "{}: In function '{}':", loc.file_name(), loc.function_name());
        std::println(std::cerr, "{}:{}:{}: assertion failed: {}", loc.file_name(), loc.line(), loc.column(), msg);
        std::terminate();
    }
    // #endif
}
