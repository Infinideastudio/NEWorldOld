# NEWorld

NEWorld is an open-source game with similar game rules to Minecraft.  

* Build commands (requires CMake 3.28+):
  ```sh
  cmake -S . -B build
  cmake --build build
  ```
  This should automatically download and build all external dependencies, including `glfw3`, `utfcpp` and `freetype`.
* Alternatively, use [vcpkg](https://learn.microsoft.com/en-us/vcpkg/) to find and download packages by passing `-DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake` to the first command.
* Alternatively, use [xmake](https://xmake.io/) to build the project:
  ```sh
  xrepo install glfw freetype utfcpp
  xmake
  ```

The compiler is set to use C++20. By default, all dependencies are statically linked.
