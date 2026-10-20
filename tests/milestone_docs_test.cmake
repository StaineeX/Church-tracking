# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
# Milestone contract: CI always runs independent tests, while the manual smoke
# procedure identifies runtime evidence and unavailable environments.
set(workflow "${SOURCE_ROOT}/.github/workflows/build.yml")
set(smoke "${SOURCE_ROOT}/docs/smoke-tests/obs-synthetic-camera.md")
set(real_smoke "${SOURCE_ROOT}/docs/smoke-tests/obs-real-camera.md")
set(readme "${SOURCE_ROOT}/README.md")
set(building "${SOURCE_ROOT}/docs/development/building.md")
set(notices "${SOURCE_ROOT}/THIRD_PARTY_NOTICES.md")
set(obs_plugin_cmake "${SOURCE_ROOT}/cmake/obs-plugin.cmake")
set(presets "${SOURCE_ROOT}/CMakePresets.json")
foreach(path IN ITEMS "${workflow}" "${smoke}" "${real_smoke}" "${readme}"
                      "${building}" "${notices}" "${obs_plugin_cmake}" "${presets}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Missing milestone evidence: ${path}")
  endif()
endforeach()

file(READ "${workflow}" ci)
string(REPLACE "\r\n" "\n" ci "${ci}")

function(require_text text needle context)
  string(FIND "${text}" "${needle}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "${context} omits required text: ${needle}")
  endif()
endfunction()

require_text("${ci}" "  push:" "Workflow trigger")
require_text("${ci}" "  pull_request:" "Workflow trigger")

string(FIND "${ci}" "  unit-tests:" unit_start)
string(FIND "${ci}" "\n  plugin-linux:" plugin_start)
if(unit_start LESS 0 OR plugin_start LESS 0 OR plugin_start LESS unit_start)
  message(FATAL_ERROR "Workflow must contain unit-tests and plugin-linux jobs in that order")
endif()
math(EXPR unit_length "${plugin_start} - ${unit_start}")
string(SUBSTRING "${ci}" "${unit_start}" "${unit_length}" unit_job)
require_text("${unit_job}" "name: OBS-independent unit and layout tests" "Unit job")
require_text("${unit_job}" "cmake --preset unit-tests" "Unit job")
require_text("${unit_job}" "cmake --build --preset unit-tests" "Unit job")
require_text("${unit_job}" "ctest --test-dir build/unit-tests --output-on-failure" "Unit job")

string(SUBSTRING "${ci}" "${plugin_start}" -1 plugin_job)
require_text("${plugin_job}" "name: OBS plugin build (Ubuntu 24.04)" "Plugin job")
require_text("${plugin_job}" "libobs-dev" "Plugin job dependency setup")
require_text("${plugin_job}" "cmake --preset plugin-linux" "Plugin job")
require_text("${plugin_job}" "cmake --build --preset plugin-linux" "Plugin job")
require_text("${plugin_job}" "cmake --install build/plugin-linux" "Plugin job")
require_text("${plugin_job}" "actions/upload-artifact@v4" "Plugin job artifact step")
require_text("${plugin_job}" "if: always()" "Plugin job artifact step")
require_text("${plugin_job}" "            build/ci-logs/configure.log" "Plugin job artifact evidence")
require_text("${plugin_job}" "            build/ci-logs/build.log" "Plugin job artifact evidence")
require_text("${plugin_job}" "            build/ci-logs/status.txt" "Plugin job artifact evidence")
require_text("${plugin_job}" "libopencv-dev" "Plugin job OpenCV dependency")
require_text("${plugin_job}" "dependencies.log" "Plugin job dependency evidence")
require_text("${plugin_job}" "BUILD: PASSED: Linux plugin compiled" "Plugin job build evidence")
require_text("${plugin_job}" "RUNTIME: UNAVAILABLE: OBS has no graphical runtime smoke session in CI" "Plugin job runtime evidence")

file(READ "${smoke}" procedure)
foreach(required IN ITEMS "church_director_filter" "Synthetic target mode"
                          "clean profile" "test pattern" "initial zoom-in"
                          "ramp" "continued pan" "continued crop"
                          "disable" "remove" "re-add" "three times"
                          "OBS log" "unavailable" "unit/layout")
  string(FIND "${procedure}" "${required}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "Smoke procedure omits milestone contract: ${required}")
  endif()
endforeach()

file(READ "${real_smoke}" real_procedure)
foreach(required IN ITEMS "church_director_filter" "clean OBS profile"
                          "camera source" "Real person tracking" "default"
                          "RGBA" "BGRA" "BGRX" "BGR3" "NV12" "I420"
                          "detection" "pan" "crop" "zoom" "multiple people"
                          "no-person" "disable" "remove" "re-add"
                          "OBS log" "preview" "unavailable" "unit/layout")
  string(FIND "${real_procedure}" "${required}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "Real-camera smoke procedure omits milestone contract: ${required}")
  endif()
endforeach()

file(READ "${readme}" readme_text)
foreach(required IN ITEMS "Real person tracking" "OpenCV" "No model download"
                          "camera source" "obs-real-camera.md")
  require_text("${readme_text}" "${required}" "README real-mode documentation")
endforeach()

file(READ "${building}" building_text)
foreach(required IN ITEMS "OpenCV" "libobs" "no model download" "Windows"
                          "macOS" "unavailable" "obs-real-camera.md")
  require_text("${building_text}" "${required}" "Build guide real-mode documentation")
endforeach()

file(READ "${notices}" notices_text)
foreach(required IN ITEMS "OpenCV 4" "Apache-2.0" "HOG" "no model weights")
  require_text("${notices_text}" "${required}" "Third-party OpenCV notice")
endforeach()

file(READ "${obs_plugin_cmake}" obs_plugin_text)
foreach(required IN ITEMS "find_package(OpenCV REQUIRED" "core" "imgproc" "objdetect"
                          "OpenCV_LIBS")
  require_text("${obs_plugin_text}" "${required}" "OBS plugin OpenCV linkage")
endforeach()

file(READ "${presets}" preset_text)
require_text("${preset_text}" "\"BUILD_OBS_PLUGIN\": \"OFF\"" "Unit preset dependency boundary")
require_text("${preset_text}" "\"BUILD_OBS_PLUGIN\": \"ON\"" "Plugin preset dependency boundary")
