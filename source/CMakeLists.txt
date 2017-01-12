cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)

aux_source_directory(./ SRC)
link_libraries(opengl32 glu32 glfw3 glew32s)
add_definitions(-DNOMINMAX)

add_executable(neworld ${SRC})
