# Church Director for OBS — Design Specification

**Status:** Approved design; real-person tracking vertical slice in implementation

**Recorded ruling:** The default vertical slice is real OpenCV HOG person tracking. Synthetic mode remains available for deterministic tests and diagnostics; the real camera smoke test is the runtime acceptance path.

**Repository:** `StaineeX/Church-tracking`

**Branch:** `feat/church-director-obs`

## Goal

Church Director for OBS is an OBS Studio plugin that keeps a live church-service camera framed on a selected subject or region by combining asynchronous detection/tracking with a bounded virtual pan/crop/zoom motion engine. Later milestones add a Bible/service browser source and a provider-agnostic speech-to-text/AI architecture.

## Context and constraints

- The target repository was empty at audit time; no product code, tests, license, or master specification was available.
- The approved first deliverable is a functional vertical slice, not the complete product.
- The current/default vertical slice is real OpenCV HOG person tracking on camera pixels. It must work without a physical camera in unit tests by using deterministic synthetic frames/targets; synthetic target mode remains available for deterministic tests and diagnostics, but is not the product default or runtime acceptance path.
- OBS rendering must never wait for detector, tracker, model loading, or I/O work.
- Worker code must not call OBS graphics APIs or retain OBS-owned frame buffers after the callback that supplied them.
- The design must preserve third-party license and attribution notices. The initial project license is expected to align with the OBS plugin template/OBS ecosystem, but distribution and dependency licensing must be reviewed before release.
- No model weights are committed until their provenance and redistribution terms are documented.

## Scope by milestone

### Milestone 1 — Smart Camera vertical slice

Deliver a native C++ OBS video filter that can be attached to an existing camera source and visibly applies a virtual crop/zoom driven by real OpenCV HOG person detections and a bounded tracker. It includes:

1. CMake/plugin bootstrap based on the official OBS plugin template.
2. Source/filter lifecycle and settings plumbing.
3. Frame sampling with a bounded, latest-frame-wins queue.
4. Detector and tracker interfaces, with OpenCV HOG person detection and PersonTracker as the default implementation plus deterministic synthetic implementations for tests and diagnostics.
5. Immutable target snapshots published from the worker.
6. Dead-zone motion engine with smoothing, speed/zoom limits, crop clamping, stale-target handling, and a neutral fallback.
7. OBS shader-based crop/pan/zoom rendering.
8. Unit tests for geometry, motion, queue behavior, and worker lifecycle.
9. A build check and a documented real-camera OBS smoke test that proves module loading, filter creation, person detection, and visible movement; the dependency-free test suite remains the no-camera proof for core behavior.

Milestone 1 does not require downloadable production ML weights, cloud service, camera vendor SDK, Bible UI, service UI, STT provider, or AI provider. OpenCV's built-in HOG coefficients are the real-mode detector baseline.

### Milestone 2 — Bible and service browser source

Add a browser source/dock boundary with a versioned local message protocol. Keep content rendering in HTML/CSS/JavaScript and keep OBS integration in the plugin/frontend layer. Define offline behavior, safe local asset loading, and explicit user-controlled content updates before implementation.

### Milestone 3 — STT/AI architecture

Add provider-agnostic interfaces for speech capture, transcription, intent/event extraction, and optional AI assistance. Providers are adapters; the core remains usable offline. Credentials, network access, retention, consent, and failure behavior must be specified before provider code is added.

## Architecture

```text
OBS camera source
      │
      ├── video filter render callback ──► shader crop/pan/zoom ──► OBS output
      │
      └── bounded frame copy ──► worker
                                  ├── detector
                                  ├── tracker
                                  └── motion engine
                                        │
                                  atomic latest CropState
```

The plugin is a video filter attached to a source already managed by OBS. The render callback samples only the data needed for analysis, submits an owned copy to a bounded queue, reads the newest `CropState`, and renders the parent source through a shader. If the worker is busy, old analysis frames are discarded rather than creating unbounded latency.

The worker owns detector/tracker state and publishes immutable, normalized results. The render path uses a lock-free or otherwise non-blocking snapshot mechanism whose implementation is validated by tests. A worker shutdown must stop intake, wake the worker, join it, release model/tracker resources, and leave the filter in a safe neutral-render state.

## Core data contracts

The exact names may follow the repository's established style once implementation starts, but the contracts are fixed:

```cpp
struct NormalizedRect {
    float left;    // [0, 1]
    float top;     // [0, 1]
    float right;   // [0, 1]
    float bottom;  // [0, 1]
};

struct VideoFrame {
    std::shared_ptr<const uint8_t[]> pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    PixelFormat format;
    int64_t timestamp_ns;
};

struct TargetObservation {
    NormalizedRect bounds;
    float confidence;
    int64_t timestamp_ns;
};

struct CropState {
    NormalizedRect visible_region;
    float zoom;
    bool tracking;
    int64_t timestamp_ns;
};

class Detector {
public:
    virtual ~Detector() = default;
    virtual std::vector<TargetObservation> detect(const VideoFrame&) = 0;
};

class Tracker {
public:
    virtual ~Tracker() = default;
    virtual void reset() = 0;
    virtual void set_target(TargetObservation target) = 0;
    virtual std::optional<TargetObservation> predict(std::int64_t timestamp_ns) const
    {
        return std::nullopt;
    }
    virtual std::optional<TargetObservation> update(const VideoFrame&) = 0;
};

class MotionEngine {
public:
    virtual ~MotionEngine() = default;
    virtual CropState update(std::optional<TargetObservation> target,
                             float delta_seconds) = 0;
};
```

The implementation may refine ownership and pixel-format types, but it must retain these behavioral guarantees: normalized coordinates, explicit timestamps, confidence, no borrowed frame lifetime across the callback, and an observable tracking/fallback state.

## Motion behavior

- The dead-zone is a normalized rectangle inside the current visible region.
- A target whose center remains inside the dead-zone does not move the crop.
- When the target exits the dead-zone, the engine moves the crop toward the smallest correction that brings the target back toward the zone.
- Crop coordinates are clamped so the requested visible region remains inside the source image.
- Translation and zoom change at bounded rates and are smoothed using elapsed frame time, not callback count.
- Low-confidence, missing, stale, or invalid observations do not produce a jump; the engine holds or eases toward a neutral safe state according to settings.
- Invalid rectangles, NaN/Infinity values, negative time deltas, and unreasonably large time gaps are rejected or bounded deterministically.

All numeric defaults belong in one settings/configuration type and must be covered by tests. The first implementation should expose only the controls needed to demonstrate the slice; advanced tuning UI is deferred.

## OBS integration

Use the official OBS plugin template as the build foundation. The plugin module registers the filter in `obs_module_load()` and releases all references/resources during unload and filter destruction. The filter uses OBS's standard filter processing helpers and shader/effect path for rendering.

The first implementation should target the current template-supported development environments where available: Windows with Visual Studio 2022, macOS with Xcode, and Linux with CMake/Ninja. CI configuration should make unsupported local environments explicit rather than silently claiming a successful OBS build.

The plugin must degrade safely when:

- the parent source is absent or not renderable;
- the shader/effect cannot be loaded;
- the worker has no current observation;
- frame format or dimensions are unsupported;
- the detector/tracker initialization fails.

In those cases it renders the parent unchanged when possible, logs an actionable error once per failure class, and does not crash OBS.

## Testing strategy

### Unit tests

Run without OBS, GPU, camera, model weights, or network. Cover:

- dead-zone hold and correction;
- crop bounds and zoom limits;
- smoothing under fixed timestamps;
- low confidence, stale target, target loss, invalid geometry, and time anomalies;
- latest-frame-wins queue behavior under a slow worker;
- immutable snapshot publication and clean worker shutdown.

### Integration/smoke tests

- Configure and build the plugin with the supported local OBS dependency path or CI workflow.
- Load the module in an OBS test profile.
- Add the filter to a deterministic test source.
- Confirm the filter renders the source unchanged in neutral mode.
- With a camera source and visible person, confirm the default HOG detection and bounded crop/pan/zoom follow the target without blocking the render path.
- Keep synthetic target mode explicit and use it only for deterministic tests or diagnostics.
- Stop/remove the filter repeatedly and confirm clean shutdown without crashes or leaked worker threads.

If the host cannot run OBS or its graphics backend, report the exact unavailable check; do not replace it with a claim based only on compiling the C++ unit tests.

## Later interfaces

Milestone 2 will expose a local, versioned event/message boundary shared by a dock and browser source. The initial message families are `service_state`, `bible_content`, `overlay_visibility`, and `diagnostic_status`; schemas and security rules are to be defined in the milestone-specific spec.

Milestone 3 will define interfaces for `AudioInput`, `Transcriber`, `IntentExtractor`, and `AiAssistant`, with explicit provider capability/error states. No cloud provider is part of this specification.

## Risks and decisions

1. **Missing master Markdown:** the prior conversation and local filesystem did not contain the artifact. This spec reconstructs only the approved scope and records assumptions so they can be amended before implementation.
2. **Filter versus custom source:** use a filter for Milestone 1 because it composes with any existing OBS camera source and keeps capture/device concerns out of the first slice. A custom source remains a future option if the product later needs to own camera acquisition.
3. **Detector dependency:** real OpenCV HOG detection and PersonTracker are the current vertical slice/default because the user explicitly requested real person tracking and approved this design. Synthetic detector/tracker implementations remain for deterministic tests and diagnostics; they do not define the product default. OpenCV's built-in coefficients avoid model downloads and cloud dependencies.
4. **No GPU work on worker:** graphics operations stay on OBS's render path; the worker publishes only CPU-side state.
5. **No external writes yet:** the branch is local and has no upstream commit or pull request. Push, PR creation, and merge remain separate actions after a stable milestone and explicit user direction at that point.

## Definition of done for Milestone 1

Milestone 1 is ready for review only when all of the following have evidence:

- the plugin configures and builds in at least one supported environment;
- unit tests pass with zero failures and cover every motion/queue edge case listed above;
- an OBS real-camera smoke test demonstrates filter loading, person detection, and visible pan/crop/zoom, or the unavailable OBS check is reported precisely;
- worker shutdown and filter removal are exercised repeatedly;
- license/attribution files and dependency notes are present;
- commits are small and focused, and a review has examined the branch before a PR is proposed.
