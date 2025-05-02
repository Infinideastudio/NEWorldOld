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
add_executable(
    neworld
    "src/Chunk.cpp"
    "src/ChunkPtrArray.cpp"
    "src/ChunkRenderer.cpp"
    "src/CreateWorldMenu.cpp"
    "src/Definitions.cpp"
    "src/Framebuffer.cpp"
    "src/FrustumTest.cpp"
    "src/FunctionsKit.cpp"
    "src/GameMenu.cpp"
    "src/Globalization.cpp"
    "src/GUI.cpp"
    "src/GUIOptionsMenu.cpp"
    "src/HeightMap.cpp"
    "src/Hitbox.cpp"
    "src/LanguageMenu.cpp"
    "src/MainMenu.cpp"
    "src/NEWorld.cpp"
    "src/OptionsMenu.cpp"
    "src/Particles.cpp"
    "src/Player.cpp"
    "src/Renderer.cpp"
    "src/RenderOptionsMenu.cpp"
    "src/Setup.cpp"
    "src/Shader.cpp"
    "src/ShaderOptionsMenu.cpp"
    "src/TextRenderer.cpp"
    "src/Textures.cpp"
    "src/VertexAllocator.cpp"
    "src/World.cpp"
    "src/WorldGen.cpp"
    "src/WorldMenu.cpp"
    "src/WorldRenderer.cpp"
)
target_default_compile_options(neworld)
target_link_libraries(neworld PRIVATE glad glfw utf8cpp Freetype::Freetype)
target_link_libraries(neworld PRIVATE klsxx::essential klsxx::thread klsxx::coroutine)
target_include_directories(neworld PRIVATE "src")
target_compile_definitions(neworld PRIVATE "$<$<CONFIG:Debug,RelWithDebInfo>:NEWORLD_DEBUG>")
if(BUILD_SHARED_LIBS)
    target_compile_definitions(neworld PRIVATE "GLFW_DLL")
endif()
