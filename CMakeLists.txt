cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0135 NEW)

# ===== Enable experimental standard library modules support (CMake 3.30 and 4.0.1 only) =====
# See: https://github.com/Kitware/CMake/commits/master/Help/dev/experimental.rst
if (CMAKE_VERSION VERSION_LESS "4.0.1")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
else ()
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
endif ()

# ===== Project info =====
project(
        neworld
        VERSION "0.5.0"
        DESCRIPTION "NEWorld is an open-source voxel game with similar gameplay as Minecraft."
        HOMEPAGE_URL "https://github.com/Infinideastudio/NEWorldOld/qzr-scratch"
        LANGUAGES CXX
)

# ===== Scripts =====
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# CMake built-in script for downloading dependencies.
include(FetchContent)

# Project-specific scripts.
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Project-specific, compile options (e.g. language standard, optimization levels).
include(TargetDefaultCompileOptions)

# Project-specific, provides an option to run cppcheck.
include(CppcheckCodeAnalysis)

# ===== Dependencies =====
# See the "adding dependencies" section of README.md for instructions.

# This affects whether CMake builds static or shared libraries by default.
# See: https://cmake.org/cmake/help/latest/variable/BUILD_SHARED_LIBS.html
option(BUILD_SHARED_LIBS "Build using shared libraries (DLL)" OFF)

# The GLAD OpenGL function loader library. It provides a service for generating customized headers
# and source files for project-specific needs. We have included the generated files in this project.
# See: https://gen.glad.sh/
add_subdirectory(src/glad)

# The GLFW window handling library.
find_package(glfw3 CONFIG REQUIRED)

# The `utfcpp` Unicode library. A simple replacement for the deprecated `std::codecvt` facets.
# This is somewhat niche and lightweight, so we use `FetchContent()` to build from source.
FetchContent_Declare(
        utfcpp
        GIT_REPOSITORY https://github.com/nemtrif/utfcpp
        GIT_TAG v4.0.6
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(utfcpp)

# The `spdlog` logging library.
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# The `libpng` PNG encoder/decoder library.
find_package(PNG REQUIRED)

# The FreeType font rendering library.
find_package(Freetype REQUIRED)

# The LevelDB key-value storage library.
find_package(leveldb CONFIG REQUIRED)

# The C++ utilities library for this project.
FetchContent_Declare(
        klsxx
        GIT_REPOSITORY https://github.com/NEWorldProject/klsxx
        GIT_TAG main
        GIT_SHALLOW TRUE
)
set(KLS_VCPKG_FORCE_OVERRIDE ON)
set(KLS_DISABLE_TEST ON)
FetchContent_MakeAvailable(klsxx)

# ===== Targets =====
add_executable(neworld)
target_sources(
        neworld
        PRIVATE FILE_SET module_interfaces TYPE CXX_MODULES FILES
        "src/types.ixx"
        "src/math/mod.ixx"
        "src/math/vector.ixx"
        "src/math/matrix.ixx"
        "src/math/euler.ixx"
        "src/math/aabb.ixx"
        "src/math/frustum.ixx"
        "src/render/mod.ixx"
        "src/render/types.ixx"
        "src/render/attrib_layout.ixx"
        "src/render/attrib_builder.ixx"
        "src/render/block_layout.ixx"
        "src/render/block_builder.ixx"
        "src/render/image.ixx"
        "src/render/buffer.ixx"
        "src/render/texture.ixx"
        "src/render/framebuffer.ixx"
        "src/render/vertex_array.ixx"
        "src/render/program.ixx"
        "src/ui/mod.ixx"
        "src/ui/layout.ixx"
        # "src/ui/controls.ixx"
        "src/blocks.ixx"
        "src/chunks.ixx"
        "src/chunk_pointer_arrays.ixx"
        "src/commands.ixx"
        "src/debug.ixx"
        "src/globalization.ixx"
        "src/globals.ixx"
        "src/gui.ixx"
        "src/menus.ixx"
        "src/neworld.ixx"
        "src/height_maps.ixx"
        "src/particles.ixx"
        "src/rendering.ixx"
        "src/setup.ixx"
        "src/text_rendering.ixx"
        "src/textures.ixx"
        "src/terrain_generation.ixx"
        "src/worlds/mod.ixx"
        "src/worlds/forward.ixx"
        "src/worlds/worlds.ixx"
        "src/worlds/player.ixx"
        "src/worlds/world_rendering.cpp"
        "src/worlds/chunk_rendering.cpp"
        "src/worlds/player_impl.cpp"
        "src/items.ixx"
        PRIVATE
        "src/menus/main_menu.cpp"
        "src/menus/options_menu.cpp"
        "src/menus/render_options_menu.cpp"
        "src/menus/shader_options_menu.cpp"
        "src/menus/ui_options_menu.cpp"
        "src/menus/language_menu.cpp"
        "src/menus/world_menu.cpp"
        "src/menus/create_world_menu.cpp"
        "src/menus/game_menu.cpp"
)
target_default_compile_options(neworld)
set_target_properties(neworld PROPERTIES CXX_MODULE_STD 1)
target_link_libraries(neworld PRIVATE glad glfw utf8cpp spdlog::spdlog PNG::PNG Freetype::Freetype leveldb::leveldb)
target_link_libraries(neworld PRIVATE klsxx::essential klsxx::thread klsxx::coroutine klsxx::io)
target_include_directories(neworld PRIVATE "src")
target_compile_definitions(neworld PRIVATE "$<$<CONFIG:Debug,RelWithDebInfo>:NEWORLD_DEBUG>")
if (BUILD_SHARED_LIBS)
    target_compile_definitions(neworld PRIVATE "GLFW_DLL")
endif ()
