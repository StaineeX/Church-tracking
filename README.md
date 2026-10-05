# Church Director for OBS

Church Director is an OBS Studio video filter for camera framing during live services. This milestone uses a deterministic synthetic target stream to drive a virtual crop and zoom over an existing OBS source. It does not analyze camera pixels.

## Unit tests

Requirements: CMake 3.21 or newer, Ninja, and a C++20 compiler. These checks do not require OBS, a GPU, a camera, models, or network access. The suite includes the OBS-independent filter-state seam and an install-layout fixture.

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

## OBS plugin build

Use the host-specific `plugin-windows`, `plugin-linux`, or `plugin-macos` preset where its generator is available. Install the OBS development package for your platform first. The plugin presets turn `BUILD_OBS_PLUGIN=ON`; configuration fails if the generator or `libobs` development package is unavailable. Unit tests and the layout fixture do not constitute a real plugin build or OBS smoke test.

The registered video filter id is `church_director_filter`. Add it to an existing video source, leave **Synthetic target mode** enabled, and use the OBS source preview to observe the crop following a timestamp-driven synthetic target. Disabling the filter bypasses the crop and stops synthetic work. On Windows and Linux, the effect and English locale are installed under the module data directory. On macOS, the module is installed as `church-director.plugin` with those resources inside `Contents/Resources/effects` and `Contents/Resources/locale`, matching the OBS plugin bundle layout. Linux modules use `${CMAKE_INSTALL_LIBDIR}/obs-plugins` by default, including configured multiarch library directories; override it with `CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION` when packaging requires a different destination. Minimum zoom is fixed at `1.0` by the current motion engine; the other motion controls are editable. A camera-pixel detector and an OBS runtime smoke test remain outside this task.

## Licensing

This project is licensed under GNU GPL version 2 or (at your option) any later version. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full terms and template attribution.
