# Task 4 report — OBS filter adapter and virtual crop shader

## Status

Implemented on `feat/church-director-obs` against Task 3 commit `943ffb7`, with the OBS-independent seam and install-layout tests passing. A real OBS plugin build and runtime smoke test were unavailable on this Windows host. The adapter and shader have therefore not been compiled or exercised by OBS. This is a conditional implementation report, not proof that the filter loads in OBS.

## Scope delivered

- Added `src/obs/filter_state.hpp/.cpp`, an OBS-independent seam owning the Task 3 `TrackingWorker`. Synthetic callbacks submit only a tiny owned placeholder frame with an OBS frame timestamp; disabled and non-synthetic modes start no worker and submit no motion work. Snapshot reads return the latest `CropState`, or neutral state when unavailable.
- Added `src/obs/church_director_filter.hpp/.cpp` and `plugin_main.cpp`. The module registers the stable id `church_director_filter`; lifecycle, settings/defaults/properties, parent checks, shader effect, and filter begin/end callbacks are wired. Settings changes stop and join the prior worker on the update/lifecycle callback. The render callback submits via the worker's non-blocking latest-frame queue and reads a snapshot; it does not join or wait for detection/tracking. Graphics effect creation and uniforms run on the OBS graphics/render thread; destruction is queued to OBS's graphics task.
- Added the effect with `image`, `visible_region`, and `zoom` uniforms. It maps output UVs into the bounded visible region, clamps UVs, and uses a clamp sampler. Added English locale strings.
- Added install rules for module, effect, and locale; updated README for synthetic-only behavior and build requirements. The existing GPL headers and `THIRD_PARTY_NOTICES.md` were preserved.
- No camera-pixel detector, model, OpenCV, ONNX, Bible, STT, or AI code was added.

## TDD evidence

RED, before implementation:

```text
cmake --preset unit-tests                                   exit 0
cmake --build --preset unit-tests                           exit 1
  fatal error: obs/filter_state.hpp: No such file or directory
ctest --test-dir build/unit-tests --output-on-failure -R plugin_layout   exit 1
  OBS effect or locale asset is missing
ctest --test-dir build/unit-tests --output-on-failure        exit 1
  4/6 passed; filter_state not run (missing executable), plugin_layout failed
```

GREEN, final sequential verification:

```text
cmake --build --preset unit-tests                            exit 0; ninja: no work to do
ctest --test-dir build/unit-tests --output-on-failure -R "filter_state|plugin_layout"
                                                             exit 0; 2/2 passed
ctest --test-dir build/unit-tests --output-on-failure        exit 0; 6/6 passed
git diff --cached --check                                    exit 0; no whitespace errors
```

The layout test configures, builds, and installs a dependency-free fixture module using the production install function, then checks the Windows module, effect, and locale paths. It does not compile the OBS adapter or shader. The seam test constructs a synthetic filter without graphics, checks the neutral initial crop, checks that disabled/non-synthetic states submit no work, and observes a worker-published synthetic crop.

## OBS configure/build evidence

The supported Windows preset was attempted:

```text
cmake --preset plugin-windows                               exit 1
CMake Error at CMakeLists.txt:2 (project):
  Generator

    Visual Studio 17 2022

  could not find any instance of Visual Studio.

-- Configuring incomplete, errors occurred!
```

A Ninja configure attempt isolated the missing OBS dependency:

```text
cmake -S . -B build/plugin-dependency-check -G Ninja -DBUILD_OBS_PLUGIN=ON -DBUILD_TESTING=OFF
                                                             exit 1
CMake Error at cmake/obs-plugin.cmake:8 (find_package):
  By not providing "Findlibobs.cmake" in CMAKE_MODULE_PATH this project has
  asked CMake to find a package configuration file provided by "libobs", but
  CMake did not find one.

  Could not find a package configuration file provided by "libobs" with any
  of the following names:

    libobs.cps
    libobsConfig.cmake
    libobs-config.cmake

  Add the installation prefix of "libobs" to CMAKE_PREFIX_PATH or set
  "libobs_DIR" to a directory containing one of the above files.  If "libobs"
  provides a separate development package or SDK, be sure it has been
  installed.
Call Stack (most recent call first):
  CMakeLists.txt:29 (church_director_add_obs_plugin)

-- Configuring incomplete, errors occurred!
```

No plugin build or OBS smoke test succeeded, and no shader compilation result is claimed.

## Diff inspection and concerns

The staged diff contains only the Task 4 adapter, shader, locale, packaging, README, and focused tests (13 files; 588 insertions, 5 deletions). The pre-existing untracked `.superpowers` material was not staged. The pre-existing `src/plugin.cpp` bootstrap stub remains in the tree but is no longer compiled by the plugin target.

Known correctness concern: the macOS install branch currently places a loose module under `obs-plugins` and assets under `share/obs/obs-plugins/church-director`, whereas the official OBS plugin template uses a `.plugin` bundle with bundled resources. macOS packaging is therefore likely incompatible and was not verified here. The Windows fixture proves only the Windows install layout. The adapter's C++ compilation, effect shader compilation, and filter behavior in OBS also remain unverified until a host with Visual Studio/OBS development dependencies is used.

## Commit

The original Task 4 implementation was committed with the brief's exact message: `feat: add OBS smart camera filter and crop shader`.

## Fix round 1

The review findings were reproduced against the committed Task 4 tree before changes:

- The render callback combined a missing parent and zero-sized parent in one branch and called `obs_source_skip_video_filter` for both. The missing-parent case is now an explicit immediate return before any OBS skip helper. The layout test statically checks that the return occurs before the first skip-helper call in that path.
- The macOS installer previously emitted a loose module and external resources. It now marks the target as `BUNDLE` with `BUNDLE_EXTENSION plugin`, adds the effect and locale as bundle resources with `MACOSX_PACKAGE_LOCATION`, and installs the resulting `.plugin` bundle at the prefix root, following the official OBS template pattern.
- The Linux installer previously used `lib/obs-plugins`. It now includes `GNUInstallDirs` and defaults `CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION` to `${CMAKE_INSTALL_LIBDIR}/obs-plugins`, allowing configured multiarch library directories or an explicit cache override. Linux data uses `${CMAKE_INSTALL_DATAROOTDIR}` as well.
- The layout fixture now passes configured Linux module/data destinations from CMake, checks the generated install manifest in addition to staged files, and statically checks the render and platform packaging contracts. It remains an install-layout check and makes no OBS runtime-discovery claim.

Fix-round RED evidence:

```text
cmake --preset unit-tests; ctest --test-dir build/unit-tests --output-on-failure -R plugin_layout
exit 1
  missing-parent render path must return before skip helper
```

Fix-round GREEN evidence:

```text
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure -R "filter_state|plugin_layout"
exit 0; 2/2 passed
```

The known environment limitation is unchanged: no Visual Studio 2022 instance and no `libobs` package configuration are available, so the adapter and shader remain uncompiled by a real OBS target. No Task 5 files were changed.
