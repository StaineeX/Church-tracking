cmake_minimum_required(VERSION 3.21)
project(church_director VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_OBS_PLUGIN "Build the OBS Studio plugin module" OFF)
option(BUILD_TESTING "Build the OBS-independent unit tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenCV QUIET COMPONENTS core imgproc objdetect)

function(church_director_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

set(CHURCH_DIRECTOR_CORE_SOURCES
  src/core.cpp
  src/core/geometry.cpp
  src/core/motion_engine.cpp
  src/core/person_tracker.cpp
  src/core/tracking_worker.cpp
)
if(OpenCV_FOUND)
  list(APPEND CHURCH_DIRECTOR_CORE_SOURCES src/core/opencv_person_detector.cpp)
endif()

add_library(church_director_core STATIC ${CHURCH_DIRECTOR_CORE_SOURCES})
set_target_properties(church_director_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(church_director_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(church_director_core PUBLIC Threads::Threads)
if(OpenCV_FOUND)
  target_include_directories(church_director_core PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(church_director_core PRIVATE ${OpenCV_LIBS})
endif()
church_director_enable_warnings(church_director_core)

if(BUILD_OBS_PLUGIN)
  include(cmake/obs-plugin.cmake)
  church_director_add_obs_plugin(church_director_plugin)
  church_director_install_obs_plugin(church_director_plugin "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
