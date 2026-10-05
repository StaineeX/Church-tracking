cmake_minimum_required(VERSION 3.21)
project(church_director VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_OBS_PLUGIN "Build the OBS Studio plugin module" OFF)
option(BUILD_TESTING "Build the OBS-independent unit tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(church_director_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_library(church_director_core STATIC src/core.cpp)
target_include_directories(church_director_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
church_director_enable_warnings(church_director_core)

if(BUILD_OBS_PLUGIN)
  include(cmake/obs-plugin.cmake)
  church_director_add_obs_plugin(church_director_plugin)
endif()

if(BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
