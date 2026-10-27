# PTZ tracking safety implementation plan

## Goal

Add an explicit OBS hotkey arm/disarm control and sticky spatial target
association without changing the asynchronous detector/render architecture.

## Tasks

- [x] Add failing core regression for a distant higher-confidence distractor.
- [x] Add failing layout assertions for the armed setting and OBS hotkey.
- [x] Implement the association gate and keep existing crossing behavior.
- [x] Implement the OBS hotkey, default-off setting, transition invalidation,
      and pass-through behavior while disarmed.
- [x] Add localized labels and update the no-camera smoke test instructions.
- [x] Run focused tests, full CTest, plugin build, install, and OBS smoke test.
- [x] Commit the shader fix and PTZ safety slice in focused commits, push the
      branch, and update the existing PR.

## Verification evidence

- Focused regression tests initially failed, then passed after the association
  gate, hotkey contract, and shader-input fixes.
- Full CTest: 9/9 tests passed.
- Windows RelWithDebInfo plugin build completed and produced
  `church-director.dll`.
- The DLL and effect were staged and installed into the local OBS 32.2.2
  installation.
- OBS loaded `church-director.dll`; the synthetic source rendered with tracking
  disarmed and armed, and the runtime log recorded `tracking ARMED` without the
  previous shader compile error.
- A PTZ controller can trigger the same parent-source OBS hotkey through its
  keyboard/macro mapping. Direct VISCA/serial/IP control remains model-specific.
