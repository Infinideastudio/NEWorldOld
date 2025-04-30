cmake_minimum_required (VERSION 3.28)
cmake_policy (SET CMP0135 NEW)

# ===== Project info =====
project (neworld
  VERSION       0.5.0
  DESCRIPTION   "NEWorld is an open-source voxel game with similar gameplay as Minecraft."
  HOMEPAGE_URL  "https://github.com/Infinideastudio/NEWorldOld/qzr-scratch"
  LANGUAGES     CXX
)

# ===== Scripts =====
list (PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include (FetchContent)
include (TargetDefaultCompileOptions)
include (CppcheckCodeAnalysis)

# ===== Dependencies =====
option (BUILD_SHARED_LIBS "Build using shared libraries (DLL)" OFF)

message (STATUS "Downloading and building dependencies...")

add_subdirectory (src/glad)

FetchContent_Declare (glfw_repo
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  FIND_PACKAGE_ARGS NAMES glfw3)

FetchContent_MakeAvailable (glfw_repo)

FetchContent_Declare (utfcpp_repo
  GIT_REPOSITORY https://github.com/nemtrif/utfcpp
  GIT_TAG v4.0.6
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  FIND_PACKAGE_ARGS NAMES utfcpp)

FetchContent_MakeAvailable (utfcpp_repo)

FetchContent_Declare (freetype_repo
  GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype
  GIT_TAG VER-2-13-3
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  FIND_PACKAGE_ARGS NAMES freetype)

FetchContent_MakeAvailable (freetype_repo)

# ===== Targets =====
add_executable (neworld
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
target_default_compile_options  (neworld)
target_link_libraries           (neworld PRIVATE glad glfw utf8cpp freetype)
target_include_directories      (neworld PRIVATE "src")
target_compile_definitions      (neworld PRIVATE "$<$<CONFIG:Debug,RelWithDebInfo>:NEWORLD_DEBUG>")
if (BUILD_SHARED_LIBS)
  target_compile_definitions    (neworld PRIVATE "GLFW_DLL")
endif ()
