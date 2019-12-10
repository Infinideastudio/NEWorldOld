cmake_minimum_required(VERSION 3.10)

project(NEWorld)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Freetype REQUIRED)

file(GLOB_RECURSE SOURCE source/*.*)
add_executable(NEWorld ${SOURCE})
target_compile_definitions(NEWorld PRIVATE -DNEWORLD_GAME)
target_include_directories(NEWorld PRIVATE source ${OPENAL_INCLUDE_DIR})
target_link_libraries(NEWorld PRIVATE OpenGL::GL GLEW::GLEW glfw3::glfw3 Freetype::Freetype)
