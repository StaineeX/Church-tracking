# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Adapted from the target setup in obsproject/obs-plugintemplate:
# https://github.com/obsproject/obs-plugintemplate/blob/3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc/CMakeLists.txt

function(church_director_add_obs_plugin target)
  find_package(libobs REQUIRED)
  add_library(${target} MODULE "${PROJECT_SOURCE_DIR}/src/plugin.cpp")
  target_link_libraries(${target} PRIVATE OBS::libobs church_director_core)
  set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "church-director")
  church_director_enable_warnings(${target})
endfunction()
