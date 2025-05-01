string(TOUPPER ${PROJECT_NAME} PREFIX)

# All options provided through this file.
option(${PREFIX}_ENABLE_MOST_WARNINGS "Enable most warnings" OFF)
option(${PREFIX}_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(${PREFIX}_ENABLE_ASAN "(MSVC/GCC/Clang) Enable address sanitizer" OFF)
option(${PREFIX}_ENABLE_UBSAN "(GCC/Clang) Enable undefined behavior sanitizer" OFF)
option(${PREFIX}_ENABLE_STDLIB_CHECKS "(Experimental) Enable standard library debug checks" OFF)
option(${PREFIX}_LD_EXTEND_STACK "(Experimental) Extend stack memory to 10MB(Linux LD)" OFF)
option(${PREFIX}_MACOS_LD_EXTEND_STACK "(Experimental) Extend stack memory to 10MB(macOS LD)" OFF)
option(${PREFIX}_LINK_EXTEND_STACK "(Experimental) Extend stack memory to 10MB(MSVC LINK)" OFF)
option(${PREFIX}_ENABLE_IPO "Enable inter-procedural(link-time) optimizations" OFF)

# See: https://cliutils.gitlab.io/modern-cmake/chapters/features/small.html
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)

# Sets default compile options for target.
# Currently supports g++/clang/cl.
function(target_default_compile_options TARGET)
  target_compile_features(${TARGET} PRIVATE cxx_std_20)
  set_target_properties(${TARGET} PROPERTIES CXX_EXTENSIONS OFF)

  # Basic options.
  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(${TARGET} PRIVATE "-stdlib=libc++")
    target_link_options(${TARGET} PRIVATE "-stdlib=libc++" "-lc++abi")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${TARGET} PRIVATE "/MP" "/EHsc")
  endif()

  # Warnings.
  if(${PREFIX}_ENABLE_MOST_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${TARGET} PRIVATE "-Wall" "-Wextra" "-Wpedantic" "-Wdeprecated" "-Wconversion" "-Wsign-conversion" "-Werror=old-style-cast")
      target_compile_options(${TARGET} PRIVATE "-Wno-nested-anon-types")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
      target_compile_options(${TARGET} PRIVATE "-Wall" "-Wextra" "-Wpedantic" "-Wdeprecated" "-Wconversion" "-Wsign-conversion" "-Werror=old-style-cast")
      target_compile_options(${TARGET} PRIVATE "-Wno-nested-anon-types")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      target_compile_options(${TARGET} PRIVATE "/W4" "/permissive-")
    else()
      message(AUTHOR_WARNING "No compiler warnings set for compiler '${CMAKE_CXX_COMPILER_ID}'.")
    endif()
  endif()

  # Warnings as errors.
  if(${PREFIX}_ENABLE_WARNINGS_AS_ERRORS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${TARGET} PRIVATE "-Werror")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
      target_compile_options(${TARGET} PRIVATE "-Werror")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      target_compile_options(${TARGET} PRIVATE "/WX")
    else()
      message(AUTHOR_WARNING "Warnings-as-errors not enabled for compiler '${CMAKE_CXX_COMPILER_ID}'.")
    endif()
  endif()

  # Standard library checks.
  if(${PREFIX}_ENABLE_STDLIB_CHECKS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_definitions(${TARGET} PRIVATE "_GLIBCXX_ASSERTIONS")
      target_compile_definitions(${TARGET} PRIVATE "_GLIBCXX_CONCEPT_CHECKS")
      target_compile_definitions(${TARGET} PRIVATE "_GLIBCXX_DEBUG")
      target_compile_definitions(${TARGET} PRIVATE "_GLIBCXX_DEBUG_PEDANTIC")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
      target_compile_definitions(${TARGET} PRIVATE "_LIBCPP_ENABLE_DEBUG_MODE")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      message(AUTHOR_WARNING "Standard library checks are controlled by build configuration.")
    else()
      message(AUTHOR_WARNING "Standard library checks not enabled for compiler '${CMAKE_CXX_COMPILER_ID}'.")
    endif()
  endif()

  # ASan.
  if(${PREFIX}_ENABLE_ASAN)
    target_compile_options(${TARGET} PRIVATE "-fsanitize=address")
    target_link_options(${TARGET} PRIVATE "-fsanitize=address")
  endif()

  # UBSan.
  if(${PREFIX}_ENABLE_UBSAN)
    target_compile_options(${TARGET} PRIVATE "-fsanitize=undefined")
    target_link_options(${TARGET} PRIVATE "-fsanitize=undefined")
  endif()

  # Extend stack memory.
  if(${PREFIX}_LD_EXTEND_STACK)
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-Wl,-z,stack-size=10000000")
  elseif(${PREFIX}_MACOS_LD_EXTEND_STACK)
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-Wl,-stack_size,10000000")
  elseif(${PREFIX}_LINK_EXTEND_STACK)
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "/STACK:10000000")
  endif()

  # Inter-procedural(link-time) optimisations.
  if(IPO_SUPPORTED AND ${PREFIX}_ENABLE_IPO)
    set_target_properties(${TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endfunction()
