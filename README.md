# Church Director for OBS

Church Director is an OBS Studio plugin project for camera framing during live services. This initial bootstrap contains the OBS-independent core library placeholder and a dependency-free test harness; plugin integration is optional.

## Unit tests

Requirements: CMake 3.21 or newer and Ninja. These checks do not require OBS, a GPU, a camera, models, or network access.

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

## OBS plugin build

Use the host-specific `plugin-windows`, `plugin-linux`, or `plugin-macos` preset where its generator is available. Install the OBS development package for your platform first. The plugin presets turn `BUILD_OBS_PLUGIN=ON`; configuration will report a missing OBS development package rather than silently producing an OBS build.

The OBS module entry point is only a bootstrap stub. Filter behavior and runtime dependencies are outside this task.

## Licensing

This project is licensed under GNU GPL version 2 or (at your option) any later version. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full terms and template attribution.
