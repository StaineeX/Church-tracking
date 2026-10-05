# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
# Milestone contract: CI always runs independent tests, while the manual smoke
# procedure identifies runtime evidence and unavailable environments.
set(workflow "${SOURCE_ROOT}/.github/workflows/build.yml")
set(smoke "${SOURCE_ROOT}/docs/smoke-tests/obs-synthetic-camera.md")
foreach(path IN ITEMS "${workflow}" "${smoke}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Missing milestone evidence: ${path}")
  endif()
endforeach()

file(READ "${workflow}" ci)
foreach(required IN ITEMS "cmake --preset unit-tests"
                          "cmake --build --preset unit-tests"
                          "ctest --test-dir build/unit-tests --output-on-failure")
  string(FIND "${ci}" "${required}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "CI omits OBS-independent command: ${required}")
  endif()
endforeach()
if(NOT ci MATCHES "plugin-linux" OR NOT ci MATCHES "libobs-dev")
  message(FATAL_ERROR "CI omits the configured Linux plugin build or OBS development dependency")
endif()

file(READ "${smoke}" procedure)
foreach(required IN ITEMS "church_director_filter" "Synthetic target mode"
                          "clean profile" "test pattern" "pan" "crop" "zoom"
                          "disable" "remove" "re-add" "three times"
                          "OBS log" "unavailable" "unit/layout")
  string(FIND "${procedure}" "${required}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "Smoke procedure omits milestone contract: ${required}")
  endif()
endforeach()
