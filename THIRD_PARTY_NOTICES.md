# Third-party notices

## OBS plugin template

The build integration pattern for the optional OBS module is adapted from the official OBS plugin template:

- Project: [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
- Revision: `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`
- Revision date: 2025-12-09 (commit timestamp 2025-12-09T15:53:14-05:00)
- Files adapted: `CMakeLists.txt` module setup, `cmake/obs-plugin.cmake`, and `.github/workflows/build.yml` (job shape from `.github/workflows/build-project.yaml`)
- License: GNU General Public License version 2; see [LICENSE](LICENSE). This project is distributed under GPL-2.0-or-later.

No template implementation files are copied verbatim. The CI workflow uses `actions/checkout@v4` and `actions/upload-artifact@v4` as build services and installs the distribution's `libobs-dev` package to compile against OBS Studio; these are build-time dependencies, not vendored source or runtime assets. OBS Studio/libobs is licensed under GPL-2.0-or-later; see [OBS Studio LICENSE](https://github.com/obsproject/obs-studio/blob/master/COPYING). Preserve applicable notices if template code is copied or adapted in later changes, and add notices for future third-party dependencies and assets. This milestone includes no OpenCV, ONNX Runtime, model weights, or cloud credentials.
