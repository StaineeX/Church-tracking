<div align="center">
  <img src="assets/church-director-logo.svg" alt="Church Director camera and cross logo" width="240">
  <h1>Church Director for OBS</h1>
  <p>Automatic, operator-controlled framing for live church services.</p>
  <p>
    <a href="docs/smoke-tests/obs-real-camera.md">Real camera test</a> ·
    <a href="docs/smoke-tests/iphone-camo-windows.md">iPhone + Camo</a> ·
    <a href="docs/development/building.md">Build guide</a>
  </p>
</div>

Church Director is an OBS Studio video filter that detects a person in a camera source and keeps that target inside a bounded virtual frame. The real-tracking path copies supported OBS frames into an asynchronous worker, uses OpenCV's built-in HOG person detector, and drives a smooth pan, crop, and zoom state.

| Area | Current behavior |
| --- | --- |
| Tracking | Real person tracking by default, with a synthetic target mode for deterministic diagnostics |
| Safety | Tracking starts disarmed and should be armed deliberately with the OBS hotkey or the exposed **Tracking armed** setting |
| Camera motion | Virtual pan/crop/zoom inside OBS; it does not send physical PTZ commands |
| Plugin id | `church_director_filter` |
| License | GNU GPL v2 or later |

## What Church Director does

- Finds a visible person in the source and follows the locked target without immediately switching to a more distant person with a higher confidence score.
- Applies bounded pan, crop, and zoom motion while preserving a dead zone and a neutral fallback when tracking is unavailable.
- Keeps frame detection off while disarmed, so the operator can choose exactly when automatic framing is active.
- Accepts RGBA, BGRA, BGRX, BGR3, NV12, and I420 frames when their planes and strides are valid; malformed or unsupported frames fail closed to neutral tracking.

### Important: this is virtual PTZ

The current plugin changes the framing of the video inside OBS. It does **not** move a physical PTZ camera. A physical PTZ controller can still be useful as a fast arm/disarm button when its macro sends the same shortcut configured in OBS, but camera movement remains manual until a separate PTZ integration is added.

## Quick start on Windows and OBS

### 1. Add a camera source

1. Open OBS and add **Video Capture Device** to the scene.
2. Select the camera, capture card, or `Camo Camera` device.
3. Confirm that the source shows a stable picture before adding the filter.

For an iPhone camera, follow [iPhone through Camo on Windows](docs/smoke-tests/iphone-camo-windows.md). Camo Studio exposes the phone as a normal OBS video-capture device.

### 2. Add Church Director to that same source

1. Right-click the camera source and choose **Filters**.
2. Add the **Church Director** effect filter.
3. Leave **Synthetic** unchecked for a real camera.
4. Leave **Enabled** checked, then start with the default motion values.

The filter must be attached to the same OBS source that contains the camera frames. A separate browser source or a different scene source will not provide frames to the detector.

### 3. Configure the tracking shortcut

1. Open **Settings → Hotkeys** in OBS.
2. Find **Toggle Church Director tracking**.
3. Assign a keyboard key that is easy to reach but unlikely to be pressed accidentally.
4. If desired, map the same shortcut to a physical PTZ controller button or to its Windows macro software.

Tracking is **disarmed by default**. Press the shortcut once to arm it and watch the status/logs; press it again to disarm before taking manual control of the camera. The filter also exposes **Tracking armed** in its properties for setup and diagnostics, so leave it off unless automatic framing is intentional. The shortcut is a safety control for the OBS filter, not a command that moves the physical PTZ head.

## Test without a camera

You can validate the filter controls and motion engine without a camera:

1. Add an image or other stable video source to OBS.
2. Add the Church Director filter.
3. Enable **Synthetic** and keep **Enabled** checked.
4. Use the tracking hotkey to arm and disarm the filter.
5. Confirm that the preview responds to the synthetic target and that disarming returns to neutral pass-through behavior.

Synthetic mode is a deterministic diagnostic path. It proves the filter, hotkey, and virtual-motion path are connected; it does not prove that a real camera can detect a person. Use the [synthetic-camera smoke test](docs/smoke-tests/obs-synthetic-camera.md) for the exact checklist.

## Safe operation

| State | What to expect | Operator action |
| --- | --- | --- |
| Disarmed | Source passes through; no frame copy or detector work is performed | Use this state for setup and manual PTZ movement |
| Armed, target found | The locked person drives bounded virtual framing | Keep the target visible and watch for unwanted scene changes |
| Armed, target stale/lost | Motion is held or returns toward neutral according to the configured stale/dead-zone behavior | Disarm, correct the shot, then re-arm deliberately |
| Wrong target | Spatial lock prevents an immediate jump to a distant higher-confidence person | Disarm and re-arm when you intentionally want to acquire someone else |

The HOG detector is CPU-based and works best with visible, upright people at a useful scale. It is not a face recognizer and may miss occluded, tiny, backlit, or unusual poses. Minimum zoom is fixed at `1.0` by the current motion engine; the other motion controls remain editable.

## Verification and troubleshooting

- **No preview change:** confirm the filter is attached to the camera source, **Enabled** is checked, and **Synthetic** is enabled when testing without a camera.
- **Real camera does not track:** leave **Synthetic** unchecked, verify that the source shows a person clearly, and run the [real-camera smoke test](docs/smoke-tests/obs-real-camera.md).
- **Hotkey appears to do nothing:** check that OBS shows a binding for **Toggle Church Director tracking**, press it while OBS has focus, and confirm that the status/log changes from `DISARMED` to `ARMED`.
- **Tracking switches people:** use the deliberate disarm/re-arm cycle to acquire another target; spatial locking intentionally resists distant higher-confidence replacements.
- **No camera available:** use [synthetic mode](docs/smoke-tests/obs-synthetic-camera.md) to verify the control and virtual-motion path, then perform the real-camera test when a source is available.
- **iPhone source missing:** complete the [Camo iPhone setup](docs/smoke-tests/iphone-camo-windows.md), prefer USB for a stable service connection, and select `Camo Camera` in OBS.

The real-camera test records detector logs, visible pan/crop motion, zoom motion only when the target framing requires it, repeated cleanup, and the fail-closed behavior. A passing unit/layout suite is not an OBS runtime result.

## Documentation map

| Guide | Use it when... |
| --- | --- |
| [Camo iPhone smoke test](docs/smoke-tests/iphone-camo-windows.md) | You want to use an iPhone as the OBS camera |
| [Real-camera smoke test](docs/smoke-tests/obs-real-camera.md) | You have a camera and want to prove real person tracking |
| [Synthetic-camera smoke test](docs/smoke-tests/obs-synthetic-camera.md) | You need deterministic validation without a camera |
| [Development build guide](docs/development/building.md) | You need to build or install the plugin on a host |

## Build and verification

The dependency-free unit preset requires CMake 3.21 or newer, Ninja, and a C++20 compiler. These checks do not require OBS, OpenCV, a GPU, a camera, models, or network access. The suite includes the OBS-independent filter-state seam and an install-layout fixture.

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

The optional Linux `opencv-tests` preset enables the OBS-independent HOG detector test. It requires OpenCV >= 4.5, but does not require OBS/libobs:

```sh
cmake --preset opencv-tests
cmake --build --preset opencv-tests
ctest --test-dir build/opencv-tests -R person_detector --output-on-failure
```

See [the development build guide](docs/development/building.md) for host-specific plugin builds and CI evidence. CI runs the unit suite on every pushed branch and pull request; the Linux plugin job records its separate dependency, build, and install outcome. A unit/layout pass does not constitute a real plugin build or an OBS smoke test.

### Host plugin notes

Use the `plugin-windows`, `plugin-linux`, or `plugin-macos` preset where its generator is available. Install OBS/libobs development files and OpenCV >= 4.5 (core, imgproc, and objdetect) for the target platform first. The plugin presets turn `BUILD_OBS_PLUGIN=ON`; configuration fails if the generator, `libobs`, or OpenCV is unavailable. No model download, network access, or cloud credential is required.

- On Windows and Linux, the effect and English locale are installed under the module data directory.
- On macOS, the module is installed as `church-director.plugin` with resources inside `Contents/Resources/effects` and `Contents/Resources/locale`, matching the OBS plugin bundle layout.
- Linux modules use `${CMAKE_INSTALL_LIBDIR}/obs-plugins` by default, including configured multiarch library directories. Override it with `CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION` when packaging requires a different destination.

## Licensing and attribution

This project is licensed under GNU GPL version 2 or, at your option, any later version. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full terms and third-party/template attribution.
