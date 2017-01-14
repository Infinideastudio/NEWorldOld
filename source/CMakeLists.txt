cmake_minimum_required(VERSION 3.1)

set(CMAKE_MACOSX_RPATH ON)
set(CMAKE_CXX_STANDARD 14)

aux_source_directory(./ SRC)
if (CMAKE_SYSTEM_NAME MATCHES Windows)
    link_libraries(opengl32 glu32 glfw3 glew32s)
    add_definitions(-DNOMINMAX -DNEWORLD_TARGET_WINDOWS)
endif()
if (CMAKE_SYSTEM_NAME MATCHES Darwin)
    link_directories(/usr/local/lib)
    include_directories(/usr/local/include)
    add_definitions(-DNEWORLD_COMPILE_DISABLE_SECURE)
    link_libraries("-framework OpenGL" glfw glew)
endif ()
if (CMAKE_SYSTEM_NAME MATCHES Linux)
    link_libraries(glu glfw3 glew)
endif ()


add_executable(neworld ${SRC})
