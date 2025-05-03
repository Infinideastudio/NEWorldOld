cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0135 NEW)

# ===== Project info =====
project(
    neworld
    VERSION "0.5.0"
    DESCRIPTION "NEWorld is an open-source voxel game with similar gameplay as Minecraft."
    HOMEPAGE_URL "https://github.com/Infinideastudio/NEWorldOld/qzr-scratch"
    LANGUAGES CXX
)

# ===== Scripts =====

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

# The FreeType font rendering library.
find_package(Freetype REQUIRED)

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
        "src/blocks.ixx"
        "src/chunks.ixx"
        "src/chunk_pointer_arrays.ixx"
        "src/commands.ixx"
        "src/framebuffers.ixx"
        "src/frustum_tests.ixx"
        "src/globalization.ixx"
        "src/gui.ixx"
        "src/menus.ixx"
        "src/neworld.ixx"
        "src/height_maps.ixx"
        "src/hitboxes.ixx"
        "src/globals.ixx"
        "src/types.ixx"
        "src/particles.ixx"
        "src/player.ixx"
        "src/shaders.ixx"
        "src/rendering.ixx"
        "src/setup.ixx"
        "src/text_rendering.ixx"
        "src/textures.ixx"
        "src/terrain_generation.ixx"
        "src/worlds.ixx"
        "src/mat4.ixx"
        "src/vec3.ixx"
    PRIVATE
        "src/chunks/chunk_rendering.cpp"
        "src/worlds/world_rendering.cpp"
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
target_link_libraries(neworld PRIVATE glad glfw utf8cpp Freetype::Freetype)
target_link_libraries(neworld PRIVATE klsxx::essential klsxx::thread klsxx::coroutine)
target_include_directories(neworld PRIVATE "src")
target_compile_definitions(neworld PRIVATE "$<$<CONFIG:Debug,RelWithDebInfo>:NEWORLD_DEBUG>")
if (BUILD_SHARED_LIBS)
    target_compile_definitions(neworld PRIVATE "GLFW_DLL")
endif ()
