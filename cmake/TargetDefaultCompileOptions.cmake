string(TOUPPER ${PROJECT_NAME} PREFIX)

# All options provided through this file.
option(${PREFIX}_ENABLE_MOST_WARNINGS "Enable most warnings" OFF)
option(${PREFIX}_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(${PREFIX}_ENABLE_ASAN "(MSVC/GCC/Clang) Enable address sanitizer" OFF)
option(${PREFIX}_ENABLE_UBSAN "(GCC/Clang) Enable undefined behavior sanitizer" OFF)
option(${PREFIX}_ENABLE_LIBCXX "(GCC/Clang) Use libc++ as the standard library" OFF)
option(${PREFIX}_ENABLE_STDLIB_HARDENING "Enable standard library hardening" ON)
option(${PREFIX}_LD_EXTEND_STACK "(Experimental) Extend stack memory to 10MB (Linux LD)" OFF)
option(${PREFIX}_MACOS_LD_EXTEND_STACK "(Experimental) Extend stack memory to 10MB (macOS LD)" OFF)
option(${PREFIX}_LINK_EXTEND_STACK "(Experimental) Extend stack memory to 10MB (MSVC LINK)" OFF)
option(${PREFIX}_ENABLE_IPO "Enable inter-procedural optimizations" ON)
option(${PREFIX}_ENABLE_PIE "Enable position independent executables" ON)

# See: https://cliutils.gitlab.io/modern-cmake/chapters/features/small.html
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
include(CheckPIESupported)
check_pie_supported()
set(PIE_SUPPORTED ${CMAKE_CXX_LINK_PIE_SUPPORTED})

# Sets default compile options for target.
# Currently supports g++/clang/cl.
function (target_default_compile_options TARGET)
    target_compile_features(${TARGET} PRIVATE cxx_std_23)
    # set_target_properties(${TARGET} PROPERTIES CXX_EXTENSIONS OFF)

    # Basic options.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${TARGET} PRIVATE "/MP" "/EHsc")
    endif ()

    # Standard library.
    if (${PREFIX}_ENABLE_LIBCXX)
        target_compile_options(${TARGET} PRIVATE "-stdlib=libc++")
        target_link_options(${TARGET} PRIVATE "-stdlib=libc++" "-lc++abi")
    endif ()

    # Warnings.
    if (${PREFIX}_ENABLE_MOST_WARNINGS)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET} PRIVATE "-Wall" "-Wextra" "-Wpedantic" "-Wdeprecated" "-Wconversion" "-Wsign-conversion" "-Werror=old-style-cast")
            target_compile_options(${TARGET} PRIVATE "-Wno-nested-anon-types")
        elseif (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
            target_compile_options(${TARGET} PRIVATE "-Wall" "-Wextra" "-Wpedantic" "-Wdeprecated" "-Wconversion" "-Wsign-conversion" "-Werror=old-style-cast")
            target_compile_options(${TARGET} PRIVATE "-Wno-nested-anon-types")
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${TARGET} PRIVATE "/W4" "/permissive-")
        else()
            message(AUTHOR_WARNING "No compiler warnings set for compiler '${CMAKE_CXX_COMPILER_ID}'.")
        endif ()
    endif ()

    # Warnings as errors.
    if (${PREFIX}_ENABLE_WARNINGS_AS_ERRORS)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET} PRIVATE "-Werror")
        elseif (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
            target_compile_options(${TARGET} PRIVATE "-Werror")
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${TARGET} PRIVATE "/WX")
        else()
            message(AUTHOR_WARNING "Warnings-as-errors not enabled for compiler '${CMAKE_CXX_COMPILER_ID}'.")
        endif ()
    endif ()

    # Standard library checks.
    if (${PREFIX}_ENABLE_STDLIB_HARDENING)
        # See: https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
        target_compile_definitions(${TARGET} PRIVATE "_GLIBCXX_CONCEPT_CHECKS")
        target_compile_definitions(${TARGET} PRIVATE "_GLIBCXX_ASSERTIONS")
        target_compile_definitions(${TARGET} PRIVATE "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG>")
        target_compile_definitions(${TARGET} PRIVATE "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG_PEDANTIC>")
        # See: https://libcxx.llvm.org/Hardening.html
        target_compile_definitions(${TARGET} PRIVATE "_LIBCPP_HARDENING_MODE=$<IF:$<CONFIG:Debug>,_LIBCPP_HARDENING_MODE_DEBUG,_LIBCPP_HARDENING_MODE_EXTENSIVE>")
        # See: https://github.com/microsoft/STL/wiki/STL-Hardening
        target_compile_definitions(${TARGET} PRIVATE "_MSVC_STL_HARDENING=1")
        target_compile_definitions(${TARGET} PRIVATE "_MSVC_STL_DESTRUCTOR_TOMBSTONES=1")
    endif ()

    # ASan.
    if (${PREFIX}_ENABLE_ASAN)
        target_compile_options(${TARGET} PRIVATE "-fsanitize=address")
        target_link_options(${TARGET} PRIVATE "-fsanitize=address")
    endif ()

    # UBSan.
    if (${PREFIX}_ENABLE_UBSAN)
        target_compile_options(${TARGET} PRIVATE "-fsanitize=undefined")
        target_link_options(${TARGET} PRIVATE "-fsanitize=undefined")
    endif ()

    # Extend stack memory.
    if (${PREFIX}_LD_EXTEND_STACK)
        set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-Wl,-z,stack-size=10000000")
    elseif (${PREFIX}_MACOS_LD_EXTEND_STACK)
        set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-Wl,-stack_size,10000000")
    elseif (${PREFIX}_LINK_EXTEND_STACK)
        set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "/STACK:10000000")
    endif ()

    # Inter-procedural optimisations.
    if (IPO_SUPPORTED AND ${PREFIX}_ENABLE_IPO)
        set_target_properties(${TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()

    # Position independent executables.
    if (PIE_SUPPORTED AND ${PREFIX}_ENABLE_PIE)
        set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif ()
endfunction ()
