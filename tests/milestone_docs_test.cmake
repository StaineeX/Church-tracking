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
set(spec "${SOURCE_ROOT}/docs/superpowers/specs/2026-10-04-church-director-obs-design.md")
set(plan "${SOURCE_ROOT}/docs/superpowers/plans/2026-10-19-real-person-tracking.md")
set(safety_spec "${SOURCE_ROOT}/docs/superpowers/specs/2026-10-26-ptz-tracking-safety-design.md")
set(safety_plan "${SOURCE_ROOT}/docs/superpowers/plans/2026-10-26-ptz-tracking-safety.md")
set(obs_plugin_cmake "${SOURCE_ROOT}/cmake/obs-plugin.cmake")
set(presets "${SOURCE_ROOT}/CMakePresets.json")
foreach(path IN ITEMS "${workflow}" "${smoke}" "${real_smoke}" "${readme}"
                      "${building}" "${notices}" "${spec}" "${plan}"
                      "${safety_spec}" "${safety_plan}"
                      "${obs_plugin_cmake}" "${presets}")
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

require_text("${ci}" "  opencv-tests:" "OpenCV detector test job")
require_text("${ci}" "name: OBS-independent OpenCV detector tests" "OpenCV detector test job")
require_text("${ci}" "cmake --preset opencv-tests" "OpenCV detector test job")
require_text("${ci}" "cmake --build --preset opencv-tests" "OpenCV detector test job")
require_text("${ci}" "ctest --test-dir build/opencv-tests -R person_detector --output-on-failure" "OpenCV detector test job")

file(READ "${smoke}" procedure)
foreach(required IN ITEMS "church_director_filter" "Synthetic target mode"
                          "perfil OBS limpo" "padrão de teste" "rampa inicial de"
                          "rampa" "pan" "crop" "Desative" "remova"
                          "adicione-o novamente" "três vezes" "log"
                          "indisponível" "unitária/layout")
  string(FIND "${procedure}" "${required}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "Smoke procedure omits milestone contract: ${required}")
  endif()
endforeach()

file(READ "${real_smoke}" real_procedure)
foreach(required IN ITEMS "church_director_filter" "perfil OBS limpo"
                          "fonte de câmera" "Real person tracking" "padrão"
                          "RGBA" "BGRA" "BGRX" "BGR3" "NV12" "I420"
                          "detecção" "pan" "crop" "zoom"
                          "espere movimento de" "zoom somente quando"
                          "várias pessoas" "Windows" "macOS"
                          "permissões de dispositivo/portal"
                          "sem pessoas" "desarme" "remova"
                          "adicione-o novamente" "três vezes" "log" "prévia"
                          "indisponível" "unitária/")
  string(FIND "${real_procedure}" "${required}" offset)
  if(offset LESS 0)
    message(FATAL_ERROR "Real-camera smoke procedure omits milestone contract: ${required}")
  endif()
endforeach()

file(READ "${readme}" readme_text)
foreach(required IN ITEMS "Rastreamento real de pessoas" "OpenCV"
                          "baixar modelos" "fonte da câmera" "obs-real-camera.md"
                          "desarmado" "Toggle Church Director tracking"
                          "controle PTZ físico")
  require_text("${readme_text}" "${required}" "README real-mode documentation")
endforeach()

file(READ "${building}" building_text)
foreach(required IN ITEMS "OpenCV" "libobs" "baixar modelos" "Windows"
                          "macOS" "OpenCV >= 4.5" "Privacidade da câmera"
                          "permissões de dispositivo/portal" "indisponível"
                          "obs-real-camera.md" "Toggle Church Director tracking"
                          "botão físico da PTZ")
  require_text("${building_text}" "${required}" "Build guide real-mode documentation")
endforeach()

file(READ "${notices}" notices_text)
foreach(required IN ITEMS "OpenCV 4" "OpenCV >= 4.5" "Apache-2.0" "HOG" "no model weights"
                          "Package-specific" "does not redistribute")
  require_text("${notices_text}" "${required}" "Third-party OpenCV notice")
endforeach()

file(READ "${spec}" spec_text)
foreach(required IN ITEMS "real OpenCV HOG person tracking" "default vertical slice"
                          "Synthetic mode remains available for deterministic tests"
                          "real camera smoke test")
  require_text("${spec_text}" "${required}" "Approved real-mode design ruling")
endforeach()

file(READ "${safety_spec}" safety_spec_text)
foreach(required IN ITEMS "OBS source hotkey" "disarmed by default" "sticky spatial"
                          "not face recognition" "physical PTZ controller")
  require_text("${safety_spec_text}" "${required}" "PTZ safety design")
endforeach()

file(READ "${safety_plan}" safety_plan_text)
foreach(required IN ITEMS "armed" "distant higher-confidence distractor" "OBS hotkey"
                          "full CTest" "push the")
  require_text("${safety_plan_text}" "${required}" "PTZ safety implementation plan")
endforeach()

file(READ "${plan}" plan_text)
foreach(required IN ITEMS "real OpenCV HOG person tracking" "current default"
                          "synthetic mode remains available for deterministic tests"
                          "predicted target" "throwing detector" "frame conversion warning")
  require_text("${plan_text}" "${required}" "Real tracking review-fix plan")
endforeach()

file(READ "${obs_plugin_cmake}" obs_plugin_text)
foreach(required IN ITEMS "find_package(OpenCV 4.5 REQUIRED" "core" "imgproc" "objdetect"
                          "OpenCV_LIBS")
  require_text("${obs_plugin_text}" "${required}" "OBS plugin OpenCV linkage")
endforeach()

file(READ "${presets}" preset_text)
require_text("${preset_text}" "\"BUILD_OBS_PLUGIN\": \"OFF\"" "Unit preset dependency boundary")
require_text("${preset_text}" "\"BUILD_OBS_PLUGIN\": \"ON\"" "Plugin preset dependency boundary")
require_text("${preset_text}" "\"BUILD_OPENCV_TESTS\": \"OFF\"" "Unit preset OpenCV boundary")
require_text("${preset_text}" "\"BUILD_OPENCV_TESTS\": \"ON\"" "OpenCV test preset dependency boundary")
