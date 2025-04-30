add_rules("mode.debug", "mode.release")

add_requires("glfw", "utfcpp", "freetype")

target("glad")
    set_kind("static")
    add_files("src/glad/glad.c")
    add_includedirs("src/glad/include")

target("neworld")
    set_kind("binary")
    set_languages("c++20")
    add_files("src/*.cpp")
    add_includedirs("src")
    add_includedirs("src/glad/include")
    add_deps("glad")
    add_packages("glfw", "utfcpp", "freetype")
