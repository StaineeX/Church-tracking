# Copyright (C) 2026 Church Director contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Adapted from the target setup in obsproject/obs-plugintemplate:
# https://github.com/obsproject/obs-plugintemplate/blob/3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc/CMakeLists.txt

include(GNUInstallDirs)

set(CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins"
    CACHE PATH "Linux OBS module destination, including any multiarch lib directory")

function(church_director_add_obs_plugin target)
  find_package(libobs REQUIRED)
  add_library(${target} MODULE
    "${PROJECT_SOURCE_DIR}/src/obs/plugin_main.cpp"
    "${PROJECT_SOURCE_DIR}/src/obs/church_director_filter.cpp"
    "${PROJECT_SOURCE_DIR}/src/obs/filter_state.cpp")
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src" ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(${target} PRIVATE OBS::libobs church_director_core ${OpenCV_LIBS})
  set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "church-director")
  church_director_enable_warnings(${target})
endfunction()

function(church_director_install_obs_plugin target source_root)
  if(APPLE)
    set_target_properties(${target} PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION plugin)
    target_sources(${target} PRIVATE
      "${source_root}/data/effects/church-director.effect"
      "${source_root}/data/locale/en-US.ini")
    set_source_files_properties("${source_root}/data/effects/church-director.effect"
      PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/effects")
    set_source_files_properties("${source_root}/data/locale/en-US.ini"
      PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
    install(TARGETS ${target} BUNDLE DESTINATION ".")
  elseif(WIN32)
    set(module_dir "church-director/bin/64bit")
    set(data_dir "church-director/data")
  else()
    set(module_dir "${CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION}")
    set(data_dir "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/church-director")
    install(TARGETS ${target} LIBRARY DESTINATION "${module_dir}"
            RUNTIME DESTINATION "${module_dir}")
  endif()
  if(WIN32)
    install(TARGETS ${target} LIBRARY DESTINATION "${module_dir}"
            RUNTIME DESTINATION "${module_dir}")
    install(FILES "${source_root}/data/effects/church-director.effect"
            DESTINATION "${data_dir}/effects")
    install(FILES "${source_root}/data/locale/en-US.ini"
            DESTINATION "${data_dir}/locale")
  elseif(NOT APPLE)
    install(FILES "${source_root}/data/effects/church-director.effect"
            DESTINATION "${data_dir}/effects")
    install(FILES "${source_root}/data/locale/en-US.ini"
            DESTINATION "${data_dir}/locale")
  endif()
endfunction()
