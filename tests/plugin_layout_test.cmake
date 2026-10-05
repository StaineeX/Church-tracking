# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
# Configure a tiny dependency-free module and install it with the real plugin
# packaging function, so this check exercises CMake's install rules.
if(NOT EXISTS "${SOURCE_ROOT}/data/effects/church-director.effect" OR
   NOT EXISTS "${SOURCE_ROOT}/data/locale/en-US.ini")
  message(FATAL_ERROR "OBS effect or locale asset is missing")
endif()

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
  set(module "obs-plugins/church-director.so")
  set(data "share/obs/obs-plugins/church-director")
else()
  set(module "lib/obs-plugins/church-director.so")
  set(data "share/obs/obs-plugins/church-director")
endif()
foreach(relative IN ITEMS "${module}" "${data}/effects/church-director.effect"
                          "${data}/locale/en-US.ini")
  if(NOT EXISTS "${fixture}/stage/${relative}")
    message(FATAL_ERROR "missing installed OBS artifact: ${relative}")
  endif()
endforeach()
