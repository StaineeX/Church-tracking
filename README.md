# Church Director for OBS

Church Director is an OBS Studio video filter for camera framing during live services. Real person tracking is the default mode: it copies supported OBS camera frames into an asynchronous worker, uses OpenCV's built-in HOG person detector, and drives a bounded pan, crop, and zoom state. Synthetic target mode remains available for deterministic diagnostics and tests.

## Build and verification

Requirements for the dependency-free unit preset: CMake 3.21 or newer, Ninja, and a C++20 compiler. These checks do not require OBS, OpenCV, a GPU, a camera, models, or network access. The suite includes the OBS-independent filter-state seam and an install-layout fixture.

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

See [the build guide](docs/development/building.md) for host-specific plugin builds and CI evidence. CI runs this unit suite on every pushed branch and pull request; the Linux plugin job records its separate dependency, build, and install outcome. A passing unit/layout suite is not an OBS runtime result.

The optional Linux `opencv-tests` preset enables the OBS-independent HOG detector test. It requires OpenCV >= 4.5, but does not require OBS/libobs:

```sh
cmake --preset opencv-tests
cmake --build --preset opencv-tests
ctest --test-dir build/opencv-tests -R person_detector --output-on-failure
```

## OBS plugin build

Use the host-specific `plugin-windows`, `plugin-linux`, or `plugin-macos` preset where its generator is available. Install OBS/libobs development files and OpenCV >= 4.5 (core, imgproc, and objdetect) for your platform first. The plugin presets turn `BUILD_OBS_PLUGIN=ON`; configuration fails if the generator, `libobs`, or OpenCV is unavailable. No model download, network access, or cloud credential is required. Unit tests and the layout fixture do not constitute a real plugin build or OBS smoke test.

The registered video filter id is `church_director_filter`. Add it to a camera source and leave the default **Real person tracking** mode selected. Tracking is **disarmed by default**: assign OBS's `Toggle Church Director tracking` hotkey and map the same shortcut to a physical PTZ controller button or its Windows macro software. While disarmed, the filter passes the source through and does not copy frames or run detector work; arm it only when the operator wants automatic tracking, and disarm it before manual PTZ movement. The worker accepts RGBA/BGRA/BGRX/BGR3, NV12, and I420 frames when their planes and strides are valid; unsupported or malformed frames fail closed to neutral tracking. HOG is CPU-based and works best with visible, upright people at useful scale; it is not a face recognizer and may miss occluded, tiny, or unusual poses. The target association uses a spatial lock so a distant higher-confidence person does not immediately take over; disarm/re-arm to intentionally acquire another person. On Windows and Linux, the effect and English locale are installed under the module data directory. On macOS, the module is installed as `church-director.plugin` with those resources inside `Contents/Resources/effects` and `Contents/Resources/locale`, matching the OBS plugin bundle layout. Linux modules use `${CMAKE_INSTALL_LIBDIR}/obs-plugins` by default, including configured multiarch library directories; override it with `CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION` when packaging requires a different destination. Minimum zoom is fixed at `1.0` by the current motion engine; the other motion controls are editable. Follow [the real-camera smoke test](docs/smoke-tests/obs-real-camera.md) to record detection, visible pan/crop motion, zoom motion only when the target framing requires it, repeated cleanup, and logs. The [synthetic-camera smoke test](docs/smoke-tests/obs-synthetic-camera.md) remains useful for deterministic diagnostics.

To use an iPhone as the camera on Windows, follow the [Camo iPhone smoke test](docs/smoke-tests/iphone-camo-windows.md). Camo exposes the phone as a normal OBS video-capture device; Church Director must be attached to that same source.

## Licensing

This project is licensed under GNU GPL version 2 or (at your option) any later version. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full terms and template attribution.
