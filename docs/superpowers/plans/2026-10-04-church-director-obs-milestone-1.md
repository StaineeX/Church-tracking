# Church Director for OBS Milestone 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify a native OBS video filter that demonstrates asynchronous synthetic tracking driving bounded virtual pan/crop/zoom without blocking OBS rendering.

**Architecture:** Keep all testable behavior in an OBS-independent C++ core library. The core owns normalized geometry, motion, latest-frame transport, snapshots, and the tracking worker. A thin OBS adapter registers a video filter, renders the parent source through a shader, and submits synthetic frame metadata while the worker publishes only the latest crop state. The first plugin smoke path is deterministic and does not require a camera, model weights, OpenCV, ONNX Runtime, or network access.

**Tech Stack:** C++20, CMake 3.28+, CTest, OBS Studio `libobs`/graphics APIs, GLSL/effect shader, standard-library threading and atomics, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-04-church-director-obs-design.md`

## Global Constraints

- The first slice must work without a physical camera by using deterministic synthetic frames/targets in tests and a synthetic target mode in the plugin smoke path.
- OBS rendering must never wait for detector, tracker, model loading, or I/O work.
- Worker code must not call OBS graphics APIs or retain OBS-owned frame buffers after the callback that supplied them.
- No model weights are committed until their provenance and redistribution terms are documented.
- Milestone 1 does not require a production ML model, cloud service, camera vendor SDK, Bible UI, service UI, STT provider, or AI provider.
- Use the official OBS plugin template as the build foundation.
- The plugin must degrade safely when the parent source is absent or not renderable, the shader/effect cannot be loaded, the worker has no current observation, frame format or dimensions are unsupported, or detector/tracker initialization fails.
- Unit tests run without OBS, GPU, camera, model weights, or network.
- If the host cannot run OBS or its graphics backend, report the exact unavailable check; do not replace it with a claim based only on compiling the C++ unit tests.
- Preserve third-party license and attribution notices; dependency and distribution licensing must be reviewed before release.

## Review Focus

1. Render-path blocking or worker-to-graphics calls: the filter must render safely even when worker processing is slow or stopped. Test in `tests/tracking_worker_tests.cpp` and the OBS smoke procedure.
2. Frame ownership/lifetime: submitted frames must remain valid after the OBS callback and old work must be discarded. Test in `tests/latest_frame_queue_tests.cpp`.
3. Motion instability: invalid geometry, stale/low-confidence targets, negative or huge time deltas, and crop-boundary conditions must never create NaN, jumps, or out-of-bounds crop state. Test in `tests/motion_engine_tests.cpp`.
4. Shutdown/reconfiguration: removing the filter or destroying the worker must wake, join, and release resources without a leaked thread or use-after-free. Test in `tests/tracking_worker_tests.cpp` and repeat in the smoke procedure.
5. Plugin packaging/licensing: the module name, effect/locale paths, attribution, and CMake install layout must agree. Verify in `tests/plugin_layout_test.cmake`, CMake configure/build output, and the smoke procedure.

---

### Task 1: Project bootstrap, legal notices, and test harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `cmake/` helper files copied or adapted from `obsproject/obs-plugintemplate`, retaining applicable license headers
- Create: `tests/test_main.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `.gitignore`
- Create: `LICENSE`
- Create: `THIRD_PARTY_NOTICES.md`
- Create: `README.md`

**Interfaces:**
- Produces a CMake project with `BUILD_OBS_PLUGIN=OFF` for OBS-independent unit tests and an OBS preset with `BUILD_OBS_PLUGIN=ON` for plugin builds.
- Produces a no-dependency test executable registered with CTest; tests use a small assertion helper in `tests/test_main.cpp`, not a new third-party test framework.
- Produces a documented source of the OBS template files and the license/attribution obligations preserved by the project.

- [ ] **Step 1: Write the failing harness check**

Add a test executable that registers one placeholder test named `bootstrap_test_executable_runs` and a CMake/CTest registration for it. The test should fail initially because the test runner and `CMakeLists.txt` do not yet exist.

- [ ] **Step 2: Run the red check**

Run:

```text
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

Expected: configure/build cannot find the not-yet-created project files or test target.

- [ ] **Step 3: Implement the minimal project**

Create the root CMake project with C++20, warnings, `BUILD_OBS_PLUGIN`, `BUILD_TESTING`, a core static-library target placeholder that later tasks extend, and the unit-test target. Add a `plugin-linux`/`plugin-windows`/`plugin-macos` preset only when its generator and host condition match the official template. Keep `BUILD_OBS_PLUGIN=OFF` for `unit-tests`. Copy only the template build helpers needed by this project and record their source URL, revision/date, and license in `THIRD_PARTY_NOTICES.md`.

Use GPL-2.0-or-later project headers consistent with the approved OBS/template direction, preserve the full license text, and do not add OpenCV/ONNX/model files.

- [ ] **Step 4: Run the green check**

Run the three commands above. Expected: configure succeeds, the test executable builds, and CTest reports `1/1` passing with no warnings treated as errors.

- [ ] **Step 5: Commit**

```text
git add CMakeLists.txt CMakePresets.json cmake tests .gitignore LICENSE THIRD_PARTY_NOTICES.md README.md
git commit -m "build: bootstrap Church Director OBS project"
```

### Task 2: Normalized geometry and dead-zone motion engine

**Files:**
- Create: `src/core/geometry.hpp`
- Create: `src/core/geometry.cpp`
- Create: `src/core/motion_engine.hpp`
- Create: `src/core/motion_engine.cpp`
- Create: `tests/motion_engine_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: C++20 standard library only.
- Produces: `church::NormalizedRect`, `church::TargetObservation`, `church::CropState`, `church::MotionSettings`, and `church::MotionEngine::update(std::optional<TargetObservation>, float delta_seconds) -> CropState`.
- Initial default settings are exact and centralized: dead zone `{0.35f, 0.35f, 0.65f, 0.65f}`, visible region `{0,0,1,1}`, `min_zoom=1.0f`, `max_zoom=2.0f`, `max_pan_speed=1.0f` normalized units/second, `zoom_speed=0.5f`/second, confidence threshold `0.5f`, stale timeout `0.5f`.

- [ ] **Step 1: Write failing behavior tests**

Add these tests with exact behavioral assertions:

```cpp
dead_zone_holds_crop_when_target_center_is_inside_zone();
dead_zone_moves_crop_toward_target_without_exceeding_pan_speed();
crop_is_clamped_to_source_bounds_at_max_zoom();
zoom_is_limited_and_uses_elapsed_time();
low_confidence_and_stale_targets_hold_safe_state();
invalid_rectangles_and_time_values_never_publish_non_finite_state();
target_loss_eases_to_neutral_without_a_jump();
```

Use fixed timestamps/delta values and assert finite normalized coordinates, exact limit behavior, and a neutral initial `CropState`.

- [ ] **Step 2: Run the red tests**

Run:

```text
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure -R motion_engine
```

Expected: compilation fails because the core geometry and motion API do not exist.

- [ ] **Step 3: Implement the minimal motion core**

Implement finite-value validation, normalized-rectangle validity, dead-zone correction, bounded translation/zoom, source-bound clamping, stale/low-confidence handling, and time-delta clamping. Keep the engine deterministic and OBS-independent. Do not add tracker, detector, thread, shader, or UI code in this task.

- [ ] **Step 4: Run the green tests and full unit suite**

Run the targeted command, then:

```text
ctest --test-dir build/unit-tests --output-on-failure
```

Expected: all registered tests pass with zero failures.

- [ ] **Step 5: Commit**

```text
git add src/core/geometry.* src/core/motion_engine.* tests/motion_engine_tests.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add bounded dead-zone motion engine"
```

### Task 3: Latest-frame transport, immutable snapshots, and async tracking worker

**Files:**
- Create: `src/core/frame_types.hpp`
- Create: `src/core/latest_frame_queue.hpp`
- Create: `src/core/snapshot_store.hpp`
- Create: `src/core/detector.hpp`
- Create: `src/core/tracker.hpp`
- Create: `src/core/synthetic_detector.hpp`
- Create: `src/core/synthetic_tracker.hpp`
- Create: `src/core/tracking_worker.hpp`
- Create: `src/core/tracking_worker.cpp`
- Create: `tests/latest_frame_queue_tests.cpp`
- Create: `tests/tracking_worker_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `church::TargetObservation`, `church::CropState`, and `MotionEngine` from Task 2.
- Produces:
  - `church::VideoFrame` with owned pixel storage, width, height, stride, format, and `timestamp_ns`.
  - `church::LatestFrameQueue::submit(VideoFrame)` and `try_take(VideoFrame&)`, where at most the newest pending frame is retained.
  - `church::SnapshotStore<T>::publish(T)` and `read() -> std::shared_ptr<const T>` with a non-blocking read path.
  - `church::Detector::detect(const VideoFrame&) -> std::vector<TargetObservation>`.
  - `church::Tracker::reset()` and `update(const VideoFrame&) -> std::optional<TargetObservation>`.
  - `church::TrackingWorker::start()`, `submit(VideoFrame)`, `snapshot() -> std::shared_ptr<const CropState>`, and `stop()`.
- Synthetic detector behavior: from a frame timestamp, produce a deterministic target whose center follows a bounded horizontal sine-like sequence; synthetic tracker returns the last valid target between detector updates and resets after target loss. The implementation must be deterministic for identical timestamps and settings.

- [ ] **Step 1: Write failing queue, snapshot, and worker tests**

Add tests named:

```cpp
latest_frame_queue_keeps_only_newest_owned_frame();
snapshot_store_returns_last_complete_value_without_partial_updates();
worker_publishes_synthetic_crop_state_from_submitted_frames();
worker_drops_old_frames_when_processing_is_delayed();
worker_stop_unblocks_and_joins_cleanly();
worker_never_calls_graphics_or_requires_obs();
```

Use a deliberately delayed test detector/tracker to prove old frames are discarded and a scope/stop test to prove no worker thread survives destruction.

- [ ] **Step 2: Run the red tests**

Run:

```text
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure -R "latest_frame_queue|tracking_worker"
```

Expected: compilation fails because the transport and worker types do not exist.

- [ ] **Step 3: Implement the minimal async core**

Use owned frame storage and a bounded condition-variable queue for producer/worker coordination. The render-facing snapshot read must not wait for the worker; a worker may wait for new input, but `snapshot()` and queue submission from the render adapter must be bounded and non-blocking. Ensure stop closes intake, wakes the worker, joins it, and publishes no references to destroyed objects.

- [ ] **Step 4: Run targeted and full unit tests**

Run the targeted command, then the complete CTest suite. Expected: all tests pass with no thread sanitizer-style warnings from the test harness and no leaked worker threads observed by the shutdown test.

- [ ] **Step 5: Commit**

```text
git add src/core/frame_types.hpp src/core/latest_frame_queue.hpp src/core/snapshot_store.hpp src/core/detector.hpp src/core/tracker.hpp src/core/synthetic_detector.hpp src/core/synthetic_tracker.hpp src/core/tracking_worker.* tests/latest_frame_queue_tests.cpp tests/tracking_worker_tests.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add asynchronous tracking worker"
```

### Task 4: OBS filter adapter and virtual crop shader

**Files:**
- Create: `src/obs/church_director_filter.hpp`
- Create: `src/obs/church_director_filter.cpp`
- Create: `src/obs/plugin_main.cpp`
- Create: `data/effects/church-director.effect`
- Create: `data/locale/en-US.ini`
- Create: `tests/plugin_layout_test.cmake`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

**Interfaces:**
- Consumes: Task 3 `TrackingWorker` and Task 2 `CropState`; OBS `obs_source_info`, filter callbacks, module lifecycle, and graphics/effect APIs.
- Produces: a registered filter with stable id `church_director_filter`; settings for `enabled`, `synthetic_mode`, `dead_zone`, `min_zoom`, `max_zoom`, `pan_speed`, `zoom_speed`, `confidence_threshold`, and `stale_timeout`; a shader uniform contract `image`, `visible_region`, and `zoom`.
- The filter render callback must only submit owned metadata/synthetic frame input and read the latest snapshot; it must not wait for detector/tracker work.

- [ ] **Step 1: Write the failing adapter/layout tests**

Add a CMake/script-level test that checks the expected module/effect/locale install paths and a C++ seam test that constructs the filter state in synthetic mode without starting OBS graphics. Assert that the default `CropState` is neutral and disabling the filter bypasses motion work.

- [ ] **Step 2: Run the red checks**

Run the unit suite plus the layout test. Expected: the plugin target, effect, locale, and filter adapter are absent.

- [ ] **Step 3: Implement the OBS adapter**

Register the module with OBS macros, register the filter in `obs_module_load`, implement create/update/destroy/get-name/get-properties/defaults, and render the parent source through `obs_source_process_filter_begin/end` using the effect. Keep all graphics calls on the OBS render callback. Load the effect and fail safe by bypassing the filter when it is unavailable. Use synthetic mode to generate deterministic frame timestamps and target motion for the smoke path; do not claim camera-pixel detection in this milestone.

Implement `data/effects/church-director.effect` so the visible region and zoom map source texture coordinates without sampling outside the source. Keep the shader independent of model/tracker code.

- [ ] **Step 4: Run tests and configure the plugin target**

Run the unit suite, then configure the plugin preset in the available supported environment. Expected: the plugin target compiles, the layout test passes, and unit tests remain green. If OBS development dependencies are unavailable, record the exact configure failure in the report and continue only with checks that do not claim a plugin build.

- [ ] **Step 5: Commit**

```text
git add src/obs src/obs/plugin_main.cpp data tests/plugin_layout_test.cmake CMakeLists.txt README.md
git commit -m "feat: add OBS smart camera filter and crop shader"
```

### Task 5: Build workflow, smoke-test procedure, and milestone evidence

**Files:**
- Create: `.github/workflows/build.yml`
- Create: `docs/smoke-tests/obs-synthetic-camera.md`
- Create: `docs/development/building.md`
- Modify: `README.md`
- Modify: `THIRD_PARTY_NOTICES.md`

**Interfaces:**
- Consumes: the completed plugin target and CTest suite from Tasks 1–4.
- Produces: reproducible unit-test and plugin-build commands, a documented no-camera OBS smoke test, and CI artifacts/logs sufficient to distinguish unit-only verification from a real OBS smoke test.

- [ ] **Step 1: Write the failing documentation/CI checks**

Add a CI validation script/test that requires the build workflow to run the unit preset and that the smoke document names the module id, synthetic mode, expected visible behavior, cleanup repetition, and unavailable-environment reporting rule.

- [ ] **Step 2: Run the red check**

Run the validation script. Expected: it fails because the workflow and smoke documentation are absent.

- [ ] **Step 3: Implement CI and smoke evidence**

Use the official OBS template workflow shape, adapting only project names and build targets. The workflow must run the OBS-independent unit suite on every branch and attempt the supported OBS plugin build in the environments for which dependencies are configured. The smoke document must specify:

1. clean test profile;
2. deterministic test source;
3. filter insertion and synthetic mode;
4. visible pan/crop/zoom acceptance;
5. repeated disable/remove/re-add shutdown check;
6. exact logs/artifacts to preserve;
7. exact reason and scope when OBS cannot run.

Update dependency notices with the exact template revision and any newly included third-party source. Do not add model weights or cloud credentials.

- [ ] **Step 4: Run the complete milestone verification**

Run:

```text
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
git diff --check
```

Run the plugin configure/build and OBS smoke test when the host supports them. Record counts, exit codes, and unavailable checks in the implementation report. Inspect the final diff for license headers, install paths, and accidental generated files.

- [ ] **Step 5: Commit**

```text
git add .github docs README.md THIRD_PARTY_NOTICES.md
git commit -m "ci: document and verify Church Director OBS milestone"
```

## Execution and review gates

- Each task is implemented by a fresh subagent, followed by a focused spec/quality review and any required fix loop.
- The controller verifies the subagent's diff, test output, and commit independently; a subagent report is not evidence by itself.
- No task proceeds with an unresolved Critical/Important review finding.
- After Task 5, run a whole-branch review against the merge base and the approved specification.
- Do not push, open a PR, or merge into `main` until the milestone evidence and whole-branch review are clean and the user explicitly authorizes that external side effect.
