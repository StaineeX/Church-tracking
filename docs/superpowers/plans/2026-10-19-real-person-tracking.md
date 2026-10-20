# Real Person Tracking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Church Director OBS filter capture real source pixels, detect visible people, and drive a bounded pan/crop/zoom tracking state asynchronously.

**Architecture:** Keep OBS capture and pixel-format conversion in the thin adapter. Copy supported asynchronous OBS frames into the existing bounded queue, run an OpenCV HOG person detector and a constant-velocity person tracker on the worker thread, and leave the graphics render callback limited to reading the latest immutable crop snapshot and rendering the shader. Synthetic mode remains available only for deterministic tests and diagnostics.

**Tech Stack:** C++20, CMake, OBS libobs video-filter API, OpenCV core/imgproc/objdetect, CTest.

**Spec:** `docs/superpowers/specs/2026-10-04-church-director-obs-design.md`

**Recorded ruling:** real OpenCV HOG person tracking is the current default vertical slice. synthetic mode remains available for deterministic tests and diagnostics, but is not the default runtime path.

## Global Constraints

- OBS rendering must never wait for detector, tracker, model loading, or I/O work.
- No borrowed frame lifetime may cross the OBS callback; submitted frames must own their pixel storage.
- Detector results and crop state use normalized coordinates, explicit timestamps, and confidence.
- The real detector must not require model weights, network access, or cloud credentials.
- Synthetic mode must remain deterministic and must not be the default real-camera behavior.
- The worker must use a predicted target at the supplied frame timestamp for multi-person association and must preserve a valid prediction across short detection gaps.
- A detector or tracker exception must fail closed to a neutral crop and leave the worker alive for later frames.
- A failed OBS frame conversion must invalidate tracking and emit one actionable format/dimensions warning per filter instance.
- Unsupported or malformed frame formats must fail closed to neutral tracking without crashing OBS.
- Preserve GPL-2.0-or-later licensing and document OpenCV as a runtime/build dependency.

## Review Focus

- A camera frame arrives as RGBA/BGRA/BGRX/BGR3, NV12, or I420 with padded strides; conversion must copy correctly or reject safely.
- A frame has null planes, zero dimensions, an undersized stride, or a timestamp beyond the signed range; no worker or render crash is allowed.
- HOG returns multiple people; the selected target must remain stable and prefer the highest-confidence candidate when no prior target exists.
- A crossing/distractor sequence must select the candidate nearest the PersonTracker predicted target, not the stale last detection.
- Detection is temporarily absent; the tracker must predict briefly, then motion must fall back when the observation becomes stale.
- Detector/tracker processing throws a standard or unknown exception; the worker must reset motion and continue.
- OBS disables, updates, removes, and re-adds the filter while detection is busy; shutdown must join the worker without render-thread blocking.

### Task 1: Add OpenCV person detection and real target tracking

**Files:**
- Create: `src/core/opencv_person_detector.hpp`
- Create: `src/core/opencv_person_detector.cpp`
- Create: `src/core/person_tracker.hpp`
- Create: `src/core/person_tracker.cpp`
- Modify: `src/core/frame_types.hpp`
- Modify: `src/core/tracker.hpp`
- Create: `tests/person_detector_tests.cpp`
- Create: `tests/person_tracker_tests.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `OpenCvPersonDetector(float hit_threshold = 0.0f, std::uint32_t max_dimension = 640)` implements `Detector::detect(const VideoFrame&)` using `cv::HOGDescriptor::getDefaultPeopleDetector()` and returns normalized, confidence-bearing observations.
- `Tracker::predict(std::int64_t timestamp_ns)` supplies a predicted target for association with a compatibility default for existing tracker implementations.
- `PersonTracker` implements `Tracker`; `set_target` accepts a detection, `predict` returns a short constant-velocity continuation at the supplied timestamp, `update` advances the worker frame, and `reset` clears velocity and target state.
- `VideoFrame` remains an owned RGBA8 frame for the core detector boundary; invalid dimensions, stride, format, or storage return no detections.

- [x] **Step 1: Write failing detector tests** for malformed frames, normalized bounds, and a deterministic person-shaped fixture accepted by the HOG detector.
- [x] **Step 2: Run `person_detector_tests` and confirm it fails because the detector implementation is absent.**
- [x] **Step 3: Implement the detector** with bounded resize, RGBA-to-BGR conversion, HOG default people coefficients, confidence filtering, and normalized coordinate mapping.
- [x] **Step 4: Run the detector tests and the existing unit suite; confirm all pass.**
- [x] **Step 5: Write failing tracker tests** for short prediction, clamping, and reset after loss.
- [x] **Step 6: Implement `PersonTracker`** with timestamp-based center velocity and bounds clamping.
- [x] **Step 7: Run tracker and full unit tests; confirm all pass.**
- [x] **Step 8: Commit** with `feat: add real person detector and tracker`.

### Task 2: Feed real OBS frames into the asynchronous worker

**Files:**
- Modify: `src/obs/filter_state.hpp`
- Modify: `src/obs/filter_state.cpp`
- Modify: `src/obs/church_director_filter.cpp`
- Modify: `src/obs/plugin_main.cpp`
- Modify: `tests/filter_state_tests.cpp`
- Modify: `tests/plugin_layout_test.cmake`

**Interfaces:**
- `FilterState(bool enabled, bool synthetic_mode, MotionSettings, std::unique_ptr<Detector>, std::unique_ptr<Tracker>)` starts the worker for either synthetic or real mode.
- `bool FilterState::submit(VideoFrame)` accepts an owned frame without waiting for detection; `submit_synthetic` remains a test-only convenience.
- The OBS filter registers `filter_video(void*, obs_source_frame*)`, returns the original OBS frame immediately, and submits only an owned RGBA copy to the worker.

- [x] **Step 1: Add failing filter-state tests** proving real mode accepts owned frames and publishes a tracking crop through injected detector/tracker seams.
- [x] **Step 2: Run the focused test and confirm it fails with no real-frame submission API.**
- [x] **Step 3: Implement FilterState real-mode injection** and preserve synthetic-mode behavior.
- [x] **Step 4: Add failing OBS-layout assertions** for `filter_video`, default real mode, and supported format conversion branches.
- [x] **Step 5: Implement OBS frame copying** for RGBA/BGRA/BGRX/BGR3/NV12/I420, including stride/plane validation and limited-range YUV conversion; reject unsupported formats safely.
- [x] **Step 6: Wire `filter_video` and real detector/tracker construction; keep `video_render` non-blocking and remove synthetic submission from real mode.**
- [x] **Step 7: Run all OBS-independent tests and static layout checks.**
- [x] **Step 8: Commit** with `feat: connect OBS camera frames to real tracking`.

### Task 3: Make the real mode buildable, documented, and verifiable

**Files:**
- Modify: `cmake/obs-plugin.cmake`
- Modify: `CMakePresets.json`
- Modify: `.github/workflows/build.yml`
- Modify: `README.md`
- Modify: `docs/development/building.md`
- Create: `docs/smoke-tests/obs-real-camera.md`
- Modify: `THIRD_PARTY_NOTICES.md`
- Modify: `tests/milestone_docs_test.cmake`

- [x] **Step 1: Add a failing documentation/CI validation** requiring OpenCV dependency instructions, real-mode default, supported format behavior, and explicit smoke-test evidence rules.
- [x] **Step 2: Implement CMake discovery/linking** for OpenCV core/imgproc/objdetect and add `libopencv-dev` to the Linux plugin CI job; leave the unit preset dependency-free.
- [x] **Step 3: Document Windows/Linux/macOS dependency setup, HOG limitations, camera permissions, and how to capture OBS logs/preview evidence.**
- [x] **Step 4: Run the documentation test and full unit suite.**
- [x] **Step 5: Run the available plugin configure/build; if host dependencies are unavailable, record the exact command and unavailable scope instead of claiming runtime success.**
- [x] **Step 6: Commit** with `docs: document real camera tracking build and smoke test`.

### Final whole-branch review fixes

- [x] Add the timestamped prediction API and crossing/distractor regression.
- [x] Preserve PersonTracker predictions through a missed detector pass and expire after the prediction horizon.
- [x] Catch standard and unknown exceptions from a throwing detector/tracker and publish a neutral crop while continuing.
- [x] Add one-shot actionable frame conversion warning with format and dimensions, plus layout coverage.
- [x] Rename the stale locale/filter label to Church Director Smart Camera.
- [x] Record the real-mode ruling in this plan and the approved spec.

### Final verification

- [x] Run `cmake --preset unit-tests`.
- [x] Run `cmake --build --preset unit-tests`.
- [x] Run `ctest --test-dir build/unit-tests --output-on-failure`.
- [x] Run `git diff --check` and inspect `git status --short --branch`.
- [x] Run the platform plugin build when OBS/libobs/OpenCV are available.
- [x] Perform the documented OBS real-camera smoke test when OBS is available; otherwise report the exact unavailable check.
- [x] Request a final whole-branch review before considering a PR.
