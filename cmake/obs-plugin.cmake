# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Adapted from the target setup in obsproject/obs-plugintemplate:
# https://github.com/obsproject/obs-plugintemplate/blob/3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc/CMakeLists.txt

function(church_director_add_obs_plugin target)
  find_package(libobs REQUIRED)
  add_library(${target} MODULE
    "${PROJECT_SOURCE_DIR}/src/obs/plugin_main.cpp"
    "${PROJECT_SOURCE_DIR}/src/obs/church_director_filter.cpp"
    "${PROJECT_SOURCE_DIR}/src/obs/filter_state.cpp")
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
  target_link_libraries(${target} PRIVATE OBS::libobs church_director_core)
  set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "church-director")
  church_director_enable_warnings(${target})
endfunction()

function(church_director_install_obs_plugin target source_root)
  if(WIN32)
    set(module_dir "church-director/bin/64bit")
    set(data_dir "church-director/data")
  elseif(APPLE)
    set(module_dir "obs-plugins")
    set(data_dir "share/obs/obs-plugins/church-director")
  else()
    set(module_dir "lib/obs-plugins")
    set(data_dir "share/obs/obs-plugins/church-director")
  endif()
  install(TARGETS ${target} LIBRARY DESTINATION "${module_dir}"
          RUNTIME DESTINATION "${module_dir}")
  install(FILES "${source_root}/data/effects/church-director.effect"
          DESTINATION "${data_dir}/effects")
  install(FILES "${source_root}/data/locale/en-US.ini"
          DESTINATION "${data_dir}/locale")
endfunction()
