cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0135 NEW)

include(FetchContent)
FetchContent_Declare(kls_repo
        GIT_REPOSITORY https://github.com/NEWorldProject/klsxx
        GIT_TAG main
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR <nil>)
FetchContent_MakeAvailable(kls_repo)
set(KLS_VCPKG_DOWNLOAD ON)
set(KLS_DISABLE_TEST ON)
include(${kls_repo_SOURCE_DIR}/Build/Config.cmake)

# ===== Project info =====
project(neworld
        VERSION 0.5.0
        DESCRIPTION "NEWorld is an open-source voxel game with similar gameplay as Minecraft."
        HOMEPAGE_URL "https://github.com/Infinideastudio/NEWorldOld/qzr-scratch"
        LANGUAGES CXX
)

kls_configure()

# ===== Scripts =====
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(TargetDefaultCompileOptions)
include(CppcheckCodeAnalysis)

# ===== Dependencies =====
option(BUILD_SHARED_LIBS "Build using shared libraries (DLL)" OFF)

message(STATUS "Downloading and building dependencies...")

add_subdirectory(${kls_repo_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_kls_build)
kls_vcpkg_package(glfw3)
kls_vcpkg_package(freetype)
kls_vcpkg_package(utfcpp)
find_package(Freetype REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)

add_subdirectory(src/glad)

# ===== Targets =====
add_executable(neworld
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
target_link_libraries(neworld PRIVATE glad glfw utf8cpp::utf8cpp Freetype::Freetype)
target_link_libraries(neworld PRIVATE klsxx::essential klsxx::thread klsxx::coroutine)
target_include_directories(neworld PRIVATE "src")
target_compile_definitions(neworld PRIVATE "$<$<CONFIG:Debug,RelWithDebInfo>:NEWORLD_DEBUG>")
if (BUILD_SHARED_LIBS)
    target_compile_definitions(neworld PRIVATE "GLFW_DLL")
endif ()
