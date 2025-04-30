# NEWorld

NEWorld is an open-source game with similar game rules to Minecraft.

## Compiling

* Build commands (requires CMake 3.28+):
  ```sh
  cmake -S . -B build
  cmake --build build
  ```
  This will download and build the direct dependencies, including `glfw3`, `utfcpp` and `freetype`, if they are not present in the local CMake package registry.
* You may use [vcpkg](https://learn.microsoft.com/en-us/vcpkg/) to provide dependencies by passing `-DCMAKE_TOOLCHAIN_FILE=<path-to-your-vcpkg-root>/scripts/buildsystems/vcpkg.cmake` to the first command.
* If you are using **Visual Studio Code**, it can be helpful to add a `CMakeUserPresets.json` for vcpkg integration, as described in [this article](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode).
* If you are using **Visual Studio** with vcpkg integration, aforementioned steps should be happen automatically when you open the project folder.
* Alternatively, use [xmake](https://xmake.io/) to build:
  ```sh
  xrepo install glfw utfcpp freetype
  xmake
  ```

The compiler is currently set to use C++20. By default, all dependencies are statically linked.
