# NEWorld

NEWorld is an open-source game with similar game rules to Minecraft.

## Compiling

* Build commands (requires CMake 3.28+):
  ```sh
  cmake -S . -B build
  cmake --build build
  ```
  This might fail with missing dependencies, which must be installed manually or using a package manager.
* You may use the [vcpkg](https://learn.microsoft.com/en-us/vcpkg/) package manager to supply dependencies:
  ```sh
  vcpkg install
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-your-vcpkg-root>/scripts/buildsystems/vcpkg.cmake
  cmake --build build
  ```
* If you are using **Visual Studio** with vcpkg integration, aforementioned steps should be happen automatically when you open the project folder.
* If you are using **Visual Studio Code**, it can be helpful to add a `CMakeUserPresets.json` for vcpkg integration, as described in [this article](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode). Alternatively, you may add the `-DCMAKE_TOOLCHAIN_FILE=...` to the `cmake.configureArgs` setting.

The compiler is currently set to use C++23. By default, all built-from-source dependencies are statically linked.

## Current plans

* @bridgekat: rendering optimizations (screen-space refractions, GPU buffer allocation and multi-draw), code modernization (from C-with-classes style to Rust-with-shared-mutability style).

## Adding dependencies

### CMake configuration

To add an external library, you need to modify [`CMakeLists.txt`](CMakeLists.txt) first[^1].

#### Mature and popular libraries

For mature libraries that are likely to be available in all mainstream package managers, simply invoke the CMake function `find_package()`. An external package reference looks like:

```cmake
find_package(<package_name> REQUIRED)
target_link_libraries(neworld PRIVATE <package_target_name>)
```

**This can be handled by a package manager like vcpkg** if you have specified it in the command that invokes CMake configure (Visual Studio does this automatically). Note that the the two names unfortunately have subtly different meanings, and can be sometimes different:

* The `<package_name>` is the name of the package **given to CMake's module mode or config mode search procedures**[^2].
  - Module mode: a script `Find<package_name>.cmake` in certain places is used to locate the package.
  - Config mode: the package specifies its own `<package_name>Config.cmake` that is searched by CMake.
* The `<package_target_name>` is the name of the target **returned from the module mode or config mode search procedures**.
  - If module mode search succeeds, this name is specified in `Find<package_name>.cmake`.
  - If config mode search succeeds, this name is specified in `<package_name>Config.cmake`.
* An optional `CONFIG` argument can be used to enforce that only config mode search is activated. This might make the result a little bit more predictable.

#### Niche and small libraries

For niche libraries (or our own public code repositories) that are not necessarily published in all mainstream package managers, use the `FetchContent` module to build from source[^3]. An external package reference looks like:

```cmake
FetchContent_Declare(
  <some_identifier>
  GIT_REPOSITORY <git_url>
  GIT_TAG <git_tag>
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(<some_identifier>)
target_link_libraries(neworld PRIVATE <package_build_target_name>)
```

**This will download the package source code from the given repository** and compile it if it also contains a `CMakeLists.txt`. It might still fail due to missing transitive dependencies, but when the package does not have any dependencies, this could be the most predictable method. Again, the names are different this time:

* The `<some_identifier>` can be arbitrary.
* The `<package_build_target_name>` in this case is **specified in the package's `CMakeLists.txt` file**.
  - For some packages, this can be different from the `<package_target_name>` specified in `<package_name>Config.cmake`[^4].

### Package manager configuration

To instruct your package manager to automatically download dependencies, additional configuration might be necessary.

* For vcpkg, the `vcpkg.json` and `vcpkg-configuration.json` manifest files serve this purpose[^5]. With them, you may directly invoke `vcpkg install` from the command line to download and install all dependencies.
  - To add a new one, type `vcpkg add <vcpkg_package_registry_name>` to update the manifest files[^6].
  - Here the `<vcpkg_package_registry_name>` might be different from the `<package_name>` `<package_target_name>` `<package_build_target_name>` previously mentioned. This should be the name of the package listed on the [vcpkg package registry](https://vcpkg.io/en/packages).

### A comparison of names

As an example, here are what I have collected so far:

| `<package_name>` | `<package_target_name>` | `<package_build_target_name>` | `<vcpkg_package_registry_name>` |
|------------------|-------------------------|-------------------------------|---------------------------------|
| `glfw3` | `glfw` | `glfw` | `glfw3` |
| `utf8cpp` | `utf8cpp::utf8cpp` | `utf8cpp` | `utfcpp` |
| `Freetype` | `Freetype::Freetype` | `freetype` | `freetype` |





[^1]: See https://cmake.org/cmake/help/latest/guide/using-dependencies/index.html.
[^2]: See https://cmake.org/cmake/help/latest/command/find_package.html#search-modes.
[^3]: See https://cmake.org/cmake/help/latest/module/FetchContent.html.
[^4]: Therefore, even if `FetchContent()` can be set to attempt `find_package()` first using a `FIND_PACKAGE_ARGS` argument, this may not be desirable for some packages, as it does not give a consistent `<package_target_name>` across different build environments.
[^5]: See https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode.
[^6]: If importing your own code repository, see https://learn.microsoft.com/en-us/vcpkg/consume/git-registries.
