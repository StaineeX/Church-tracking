# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
# Configure a tiny dependency-free module and install it with the real plugin
# packaging function, so this check exercises CMake's install rules.
if(NOT EXISTS "${SOURCE_ROOT}/data/effects/church-director.effect" OR
   NOT EXISTS "${SOURCE_ROOT}/data/locale/en-US.ini")
  message(FATAL_ERROR "OBS effect or locale asset is missing")
endif()
file(READ "${SOURCE_ROOT}/data/locale/en-US.ini" locale_text)
if(NOT locale_text MATCHES "ChurchDirector.Filter=Church Director Smart Camera")
  message(FATAL_ERROR "OBS filter locale label must identify the Church Director Smart Camera")
endif()

file(READ "${SOURCE_ROOT}/data/effects/church-director.effect" effect_text)
foreach(contract IN ITEMS "VertData VSDefault(VertData v_in)"
                          "float4 PSCrop(VertData v_in)"
                          "vertex_shader = VSDefault(v_in);"
                          "pixel_shader = PSCrop(v_in);")
  string(FIND "${effect_text}" "${contract}" contract_offset)
  if(contract_offset LESS 0)
    message(FATAL_ERROR "OBS effect shader contract is missing: ${contract}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/obs/church_director_filter.cpp" filter_source)
string(REPLACE "\n" " " filter_source_one_line "${filter_source}")
foreach(contract IN ITEMS "filter_video(void *data, obs_source_frame *frame)"
                          "value.filter_video = filter_video;"
                          "obs_data_set_default_bool(settings, synthetic_key, false);"
                          "tracking_armed_key"
                          "obs_hotkey_register_source"
                          "filter_add"
                          "ChurchDirector.ToggleTracking"
                          "obs_source_skip_video_filter"
                          "VIDEO_FORMAT_RGBA" "VIDEO_FORMAT_BGRA"
                          "VIDEO_FORMAT_BGRX" "VIDEO_FORMAT_BGR3"
                          "VIDEO_FORMAT_NV12" "VIDEO_FORMAT_I420"
                          "frame_conversion_error_logged"
                          "unable to convert frame for tracking"
                          "format=%d" "dimensions=%ux%u"
                          "state->invalidate();")
  string(FIND "${filter_source}" "${contract}" contract_offset)
  if(contract_offset LESS 0)
    message(FATAL_ERROR "OBS filter layout is missing required contract: ${contract}")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/CMakeLists.txt" root_cmake)
foreach(contract IN ITEMS "find_package(OpenCV 4.5 REQUIRED COMPONENTS core imgproc objdetect)")
  string(FIND "${root_cmake}" "${contract}" contract_offset)
  if(contract_offset LESS 0)
    message(FATAL_ERROR "CMake is missing plugin-required OpenCV contract: ${contract}")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/cmake/obs-plugin.cmake" plugin_cmake)
foreach(contract IN ITEMS "OpenCV_INCLUDE_DIRS" "OpenCV_LIBS")
  string(FIND "${plugin_cmake}" "${contract}" contract_offset)
  if(contract_offset LESS 0)
    message(FATAL_ERROR "OBS plugin target is missing OpenCV propagation: ${contract}")
  endif()
endforeach()
string(FIND "${filter_source_one_line}"
  "obs_source_t *parent = obs_filter_get_target(filter->source);" parent_offset)
if(parent_offset LESS 0)
  message(FATAL_ERROR "render callback does not retrieve its parent source")
endif()
string(SUBSTRING "${filter_source_one_line}" "${parent_offset}" -1 parent_tail)
string(FIND "${parent_tail}" "if (!parent)" parent_guard_offset)
string(FIND "${parent_tail}" "return;" parent_return_offset)
string(FIND "${parent_tail}" "obs_source_skip_video_filter" parent_skip_offset)
if(parent_guard_offset LESS 0 OR parent_return_offset LESS 0 OR parent_skip_offset LESS 0 OR
   parent_return_offset GREATER parent_skip_offset)
  message(FATAL_ERROR "missing-parent render path must return before skip helper")
endif()

file(READ "${SOURCE_ROOT}/cmake/obs-plugin.cmake" install_source)
foreach(contract IN ITEMS "BUNDLE TRUE" "BUNDLE_EXTENSION plugin"
                          "MACOSX_PACKAGE_LOCATION" "CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION"
                          "CMAKE_INSTALL_LIBDIR")
  if(NOT install_source MATCHES "${contract}")
    message(FATAL_ERROR "install logic is missing platform contract: ${contract}")
  endif()
endforeach()

set(fixture "${TEST_BINARY_DIR}/plugin-layout-fixture")
file(REMOVE_RECURSE "${fixture}/stage")
file(MAKE_DIRECTORY "${fixture}/src")
file(WRITE "${fixture}/src/dummy.cpp" "extern \"C\" int church_director_fixture() { return 1; }\n")
file(TO_CMAKE_PATH "${SOURCE_ROOT}" source_root)
file(WRITE "${fixture}/CMakeLists.txt" "
cmake_minimum_required(VERSION 3.21)
project(plugin_layout_fixture LANGUAGES CXX)
include(\"${source_root}/cmake/obs-plugin.cmake\")
add_library(church_director_plugin MODULE src/dummy.cpp)
set_target_properties(church_director_plugin PROPERTIES PREFIX \"\" OUTPUT_NAME \"church-director\")
church_director_install_obs_plugin(church_director_plugin \"${source_root}\")
")

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${fixture}" -B "${fixture}/build"
                -G "${TEST_GENERATOR}" RESULT_VARIABLE configured OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT configured EQUAL 0)
  message(FATAL_ERROR "layout fixture configure failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${fixture}/build"
                RESULT_VARIABLE built OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT built EQUAL 0)
  message(FATAL_ERROR "layout fixture build failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${fixture}/build" --prefix "${fixture}/stage"
                RESULT_VARIABLE installed OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT installed EQUAL 0)
  message(FATAL_ERROR "layout fixture install failed: ${output}\n${error}")
endif()

if(WIN32)
  set(module "church-director/bin/64bit/church-director.dll")
  set(data "church-director/data")
elseif(APPLE)
  set(module "church-director.plugin")
  set(data "church-director.plugin/Contents/Resources")
else()
  set(module "${TEST_LINUX_MODULE_DIR}/church-director.so")
  set(data "${TEST_DATA_DIR}")
endif()
foreach(relative IN ITEMS "${module}" "${data}/effects/church-director.effect"
                          "${data}/locale/en-US.ini")
  if(NOT EXISTS "${fixture}/stage/${relative}")
    message(FATAL_ERROR "missing installed OBS artifact: ${relative}")
  endif()
endforeach()

file(READ "${fixture}/build/install_manifest.txt" install_manifest)
foreach(relative IN ITEMS "${module}" "${data}/effects/church-director.effect"
                          "${data}/locale/en-US.ini")
  file(TO_CMAKE_PATH "${fixture}/stage/${relative}" expected_path)
  if(NOT install_manifest MATCHES "${expected_path}")
    message(FATAL_ERROR "install manifest omitted expected artifact: ${relative}")
  endif()
endforeach()
