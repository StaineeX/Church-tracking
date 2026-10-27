# PTZ tracking safety controls

**Status:** Approved implementation slice

## Goal

Make Church Director safe for a church service that has a physical PTZ
controller. Tracking must be explicitly armed by the operator, quickly
disarmed when the operator needs manual PTZ control, and resistant to
switching between people in the frame.

## Decision

Use an OBS source hotkey on the public parent source as the hardware boundary. The operator maps a button
on the PTZ controller (or its Windows macro software) to one keyboard shortcut
in OBS. The same shortcut toggles tracking for the Church Director filter.
Tracking starts disarmed by default. While disarmed, the filter passes the
source through and does not copy frames or run detector work. Arming invalidates
the old crop so the worker reacquires from a neutral state.

Keep target identity out of this milestone. The worker uses a sticky spatial
association: once a person is selected, a new detection must overlap the
predicted target or remain within a bounded center-distance gate. A distant,
higher-confidence person cannot replace the current target during normal
tracking. This is deliberately not face recognition and does not claim to know
which human is which.

## Non-goals

- Direct VISCA/serial/IP support for an unknown PTZ model.
- Face recognition or biometric identification.
- Automatic camera movement through the physical PTZ controller.
- A persistent “selected person” identity after a long loss; the operator can
  disarm and re-arm to make an intentional new acquisition.

## User-facing behavior

- `Tracking armed` is visible in the filter properties and defaults to off.
- OBS Settings → Hotkeys exposes `Toggle Church Director tracking` under the
  camera/image source that owns the filter.
- The hotkey toggles the state on key-down only, logs `ARMED`/`DISARMED`, and
  resets the crop on either transition.
- The filter stays neutral/pass-through while disarmed, so a physical PTZ
  operator can take over without the plugin fighting the camera.
- The synthetic mode remains a diagnostic mode, but it also requires arming so
  the smoke test exercises the same safety gate as a real camera.

## Constraints

- Hotkey callbacks must not perform detector, graphics, or blocking work.
- OBS render callbacks must remain non-blocking.
- The detector and tracker remain asynchronous and own their copied frames.
- Existing crossing/association behavior must remain valid while distant
  distractors are rejected.

## Verification

Unit coverage will prove the distance gate, and layout coverage will prove the
new armed setting, hotkey registration, and render/video gating. The OBS smoke
test will document the default disarmed state, hotkey assignment, and a
synthetic-mode arm/disarm check that does not require a camera.
