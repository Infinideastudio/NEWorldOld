set_project("neworld")
set_policy("compatibility.version", "3.0")

set_languages("c++20")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })
add_rules("mode.debug", "mode.release", "mode.releasedbg")

add_requires("glfw", "utfcpp", "freetype")

target("glad")
    set_kind("static")
    add_files("src/glad/glad.c")
    set_encodings("utf-8")
    add_includedirs("src/glad/include", { public = true })

target("neworld")
    set_kind("binary")
    add_files("src/*.cpp")
    set_encodings("utf-8")
    add_includedirs("src")
    add_deps("glad")
    add_packages("glfw", "utfcpp", "freetype")
    set_rundir(os.projectdir())
