# Third-party notices

## OBS plugin template

The build integration pattern for the optional OBS module is adapted from the official OBS plugin template:

- Project: [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
- Revision: `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`
- Revision date: 2025-12-09 (commit timestamp 2025-12-09T15:53:14-05:00)
- Files adapted: `CMakeLists.txt` module setup, `cmake/obs-plugin.cmake`, and `.github/workflows/build.yml` (job shape from `.github/workflows/build-project.yaml`)
- License: GNU General Public License version 2; see [LICENSE](LICENSE). This project is distributed under GPL-2.0-or-later.

No template implementation files are copied verbatim. The CI workflow uses `actions/checkout@v4` and `actions/upload-artifact@v4` as build services and installs the distribution's `libobs-dev` package to compile against OBS Studio; these are build-time dependencies, not vendored source or runtime assets. OBS Studio/libobs is licensed under GPL-2.0-or-later; see [OBS Studio LICENSE](https://github.com/obsproject/obs-studio/blob/master/COPYING). Preserve applicable notices if template code is copied or adapted in later changes, and add notices for future third-party dependencies and assets.

## OpenCV 4

The real camera path depends on OpenCV 4 components `core`, `imgproc`, and `objdetect`. OpenCV 4 is distributed under the Apache License 2.0 (Apache-2.0); see the [OpenCV license](https://github.com/opencv/opencv/blob/4.x/LICENSE). It is a host build/runtime dependency discovered by CMake, not vendored source in this repository. The default HOG people detector uses coefficients provided by OpenCV, so this project ships no model weights and requires no model download, network access, or cloud credentials.

The plugin and the optional detector tests require OpenCV >= 4.5. OpenCV's baseline HOG detector is CPU-based and has ordinary limitations: it can miss tiny, occluded, side-on, or poorly lit people and can produce false positives. It is not a face recognizer or identity tracker. Report those detector limitations separately from plugin build or OBS runtime failures.

Package-specific third-party notices remain with the package that supplies the dependency. This repository does not redistribute OpenCV, OBS/libobs, or their binary packages; distro packages such as `libopencv-dev` and `libobs-dev`, Windows OBS/OpenCV installers, and macOS package-manager builds carry their own license files and notices. Preserve those notices when redistributing a package or a plugin bundle that includes its binaries, and consult the exact package metadata for the shipped build.
