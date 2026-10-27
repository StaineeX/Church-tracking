# Church Director documentation and branding design

## Intent

Make the GitHub repository approachable for a church operator who needs to
connect a camera, arm tracking safely, and verify the OBS output. Preserve the
existing technical documentation while making the README a useful first-run
guide.

## Goals

- Add an original, lightweight SVG logo showing a cross and camera/lens motif.
- Give the README a clear quick-start path for Windows, OBS, Camo/iPhone, and
  the PTZ arm/disarm shortcut.
- Keep implementation, safety limitations, smoke tests, and build instructions
  in focused linked documents rather than repeating them in full.
- Preserve GPL and third-party attribution information.

## Non-goals

- No new website, documentation generator, build dependency, or runtime code.
- No physical PTZ protocol integration in this documentation change.
- No change to the existing tracking behavior or OBS plugin binary.

## Approaches considered

1. Markdown plus an original SVG logo — selected because it is versionable,
   crisp at any size, and adds no dependency or binary asset burden.
2. A raster logo — rejected because it is heavier and less adaptable for GitHub
   sizing and dark/light backgrounds.
3. A separate documentation site — rejected because it creates a new surface to
   publish and maintain when the GitHub README and smoke tests are sufficient.

## Design

Create `assets/church-director-logo.svg` with a dark-blue rounded camera body,
gold lens ring, and a white/gold cross. Use it in the README with a centered
title block and a compact status/table section. Organize the README around:

1. What Church Director does and the virtual-PTZ limitation.
2. Quick start for Camo/iPhone and ordinary OBS camera sources.
3. Safe operation: disarmed default, source-owned toggle hotkey, PTZ mapping,
   and target-lock behavior.
4. Verification and troubleshooting links.
5. Development/build and licensing links.

The existing Camo, real-camera, and synthetic-camera smoke tests remain the
authoritative step-by-step references. The README will link to them instead of
duplicating every control.

## Acceptance checks

- Markdown links and SVG path are valid from the repository root.
- The logo renders as a standalone SVG and in the README.
- Existing documentation and unit/layout tests still pass.
- The change is committed separately and pushed to the existing PR branch.
