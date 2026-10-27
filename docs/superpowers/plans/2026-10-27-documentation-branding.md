# Church Director documentation and branding implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Church Director repository approachable for a church operator by adding an original cross-and-camera logo, restructuring the README into a clear Windows/OBS quick start, and preserving the existing safety and technical guidance.

**Architecture:** Keep the change static and repository-local: one self-contained SVG in `assets/`, one focused README rewrite, and links to the existing smoke tests and build documentation. No plugin runtime, detector, camera, PTZ, or build-system behavior changes are needed for this milestone.

**Tech Stack:** Markdown, self-contained SVG, existing CMake/CTest validation.

**Spec:** `docs/superpowers/specs/2026-10-27-readme-branding-design.md`

## Global Constraints

- Do not change runtime/plugin behavior or add dependencies.
- Preserve GPL licensing, third-party attribution, and the existing limitations around virtual pan/crop/zoom.
- Keep the default tracking safety model explicit: tracking starts disarmed, uses a deliberate hotkey, and should not be confused with physical PTZ movement.
- Link the iPhone/Camo, real-camera, synthetic-camera, and build instructions that already exist.
- Validate the documentation and existing tests before claiming completion.
- Publish a focused commit on `feat/church-director-obs` and update the existing GitHub pull request when the checks pass.

## Review Focus

- Logo path and rendering: `assets/church-director-logo.svg` must exist, be self-contained, and be referenced by the README.
- Operator safety: the README must retain disarmed-by-default behavior, the tracking toggle workflow, and the virtual-PTZ limitation.
- Camera setup accuracy: Camo must be described as the OBS video-capture source, with Church Director attached to that same source.
- Documentation quality: headings, tables, links, and quick-start steps should be readable in GitHub without introducing a second documentation system.
- Regression safety: existing CTest checks and documentation checks must continue to pass.

---

## Task 1: Add the repository logo

- [x] Create `assets/church-director-logo.svg` as an original, self-contained SVG.
- [x] Use a dark-blue camera body, a gold lens ring, and a visible white/gold cross so the logo works on GitHub's light and dark surfaces.
- [x] Avoid external fonts, images, or runtime dependencies.
- [x] Validate that the SVG has a root element, closes correctly, and contains the intended logo elements.

## Task 2: Refresh the README for operators

- [x] Add a centered logo, concise project description, status information, and a documentation map.
- [x] Add a short explanation of what the filter does and what it does not do: virtual pan/crop/zoom rather than physical PTZ commands.
- [x] Add a Windows/OBS quick start covering source creation, filter attachment, real versus synthetic mode, and the tracking hotkey.
- [x] Link the existing Camo/iPhone, real-camera, synthetic-camera, and build smoke tests.
- [x] Add a compact safe-operation and troubleshooting section covering arm/disarm state, target lock, stale detections, and no-camera validation.
- [x] Keep developer build instructions, plugin ID, test commands, and licensing information discoverable without overwhelming the first-run path.

## Task 3: Verify and publish

- [x] Run whitespace and link/path checks for the changed documentation and SVG.
- [x] Run the existing CTest suite and any repository documentation checks relevant to the change.
- [x] Review the final diff for accidental runtime or unrelated changes.
- [x] Commit the focused documentation/branding change with a descriptive message.
- [x] Push `feat/church-director-obs` and confirm the existing PR contains the update.

## Completion Evidence

Completion requires the final diff, validation output, commit identifier, and pushed branch/PR status to be reported. Do not claim the documentation milestone is complete if the tests or push fail.
