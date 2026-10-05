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
