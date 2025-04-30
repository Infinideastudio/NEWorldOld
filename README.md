# NEWorld

NEWorld is an open-source game with similar game rules to Minecraft.  

Build commands (requires CMake 3.28+):

```sh
cmake -S . -B build
cmake --build build
```

This should automatically download and build all required dependencies, including `glfw`, `utfcpp` and `freetype`. Note that the `glad`-generated source files are included in `src/glad` in this repository.

The compiler is set to use C++20. By default, all dependencies are statically linked.
