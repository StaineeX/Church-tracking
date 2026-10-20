# Building and verifying Church Director

The OBS-independent suite needs CMake 3.21+, Ninja, and a C++20 compiler. It does not need OBS, OpenCV, a camera, a GPU, network access, or model files. From the repository root:

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

This runs the core/filter-state tests, the plugin install-layout fixture, and the milestone CI/documentation check. Passing it proves those checks only. It neither compiles the real OBS module nor opens OBS.

## OBS plugin build

The real OBS plugin requires OBS Studio/libobs development files and OpenCV 4 with the `core`, `imgproc`, and `objdetect` modules. The detector uses OpenCV's built-in HOG people coefficients; no model download is required.

On Ubuntu/Linux, install the packages before configuring:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ pkg-config libobs-dev libopencv-dev
```

On Windows, install OBS Studio development/libobs files, Visual Studio 2022, and OpenCV 4 with a discoverable CMake package (for example, an OpenCV installation that provides `OpenCVConfig.cmake`). On macOS, install Xcode, OBS Studio/libobs development files, and OpenCV 4 with a discoverable CMake package (for example, Homebrew's `opencv` package). The selected CMake generator must match the host. Neither platform needs model files, a model download, or cloud credentials. Use the matching preset:

```sh
# Ubuntu/Linux with libobs development files and Ninja
cmake --preset plugin-linux
cmake --build --preset plugin-linux
cmake --install build/plugin-linux --prefix "$PWD/build/plugin-stage"
```

```powershell
# Windows with Visual Studio 2022 and a discoverable libobs CMake package
cmake --preset plugin-windows
cmake --build --preset plugin-windows
cmake --install build/plugin-windows --config RelWithDebInfo --prefix "$PWD/build/plugin-stage"
```

```sh
# macOS with Xcode and a discoverable libobs CMake package
cmake --preset plugin-macos
cmake --build --preset plugin-macos
cmake --install build/plugin-macos --config RelWithDebInfo --prefix "$PWD/build/plugin-stage"
```

The plugin is `church-director`. A successful staged install contains the module plus `effects/church-director.effect` and `locale/en-US.ini`; the `plugin_layout` test checks their platform layout with a fixture module. A real plugin build requires the actual `libobs` and OpenCV packages. If CMake cannot find `libobs` or OpenCV, or the generator/compiler is missing, record the command, exit code, and exact diagnostic as **unavailable**. Do not label a fixture build as a plugin build.

CI runs the unit preset on every pushed branch and pull request. Its separate Ubuntu 24.04 job installs `libobs-dev`, configures/builds `plugin-linux`, stages the install, and uploads logs and staged files. That job fails when dependency setup, configuration, build, or install fails; its status and logs identify what was unavailable or broken. CI has no graphical OBS session, so its artifacts are build evidence only. Windows/macOS plugin builds are manual until those environments have configured OBS development dependencies in CI.

For runtime acceptance, follow [the real-camera smoke procedure](../smoke-tests/obs-real-camera.md). Preserve its OBS log and visual evidence separately from CTest logs. The HOG detector is a baseline CPU detector: report missed detections, occlusions, tiny subjects, or false positives rather than treating them as proof that a different model was loaded.
