add_executable(church_director_tests test_main.cpp)
target_link_libraries(church_director_tests PRIVATE church_director_core)
church_director_enable_warnings(church_director_tests)
add_test(NAME bootstrap_test_executable_runs COMMAND church_director_tests)

add_executable(motion_engine_tests motion_engine_tests.cpp)
target_link_libraries(motion_engine_tests PRIVATE church_director_core)
church_director_enable_warnings(motion_engine_tests)
add_test(NAME motion_engine COMMAND motion_engine_tests)

add_executable(latest_frame_queue_tests latest_frame_queue_tests.cpp)
target_link_libraries(latest_frame_queue_tests PRIVATE church_director_core)
church_director_enable_warnings(latest_frame_queue_tests)
add_test(NAME latest_frame_queue COMMAND latest_frame_queue_tests)

add_executable(tracking_worker_tests tracking_worker_tests.cpp)
target_link_libraries(tracking_worker_tests PRIVATE church_director_core)
church_director_enable_warnings(tracking_worker_tests)
add_test(NAME tracking_worker COMMAND tracking_worker_tests)

add_executable(frame_conversion_tests frame_conversion_tests.cpp)
target_link_libraries(frame_conversion_tests PRIVATE church_director_core)
church_director_enable_warnings(frame_conversion_tests)
add_test(NAME frame_conversion COMMAND frame_conversion_tests)

add_executable(person_tracker_tests person_tracker_tests.cpp)
target_link_libraries(person_tracker_tests PRIVATE church_director_core)
church_director_enable_warnings(person_tracker_tests)
add_test(NAME person_tracker COMMAND person_tracker_tests)

if(BUILD_OPENCV_TESTS)
  add_executable(person_detector_tests person_detector_tests.cpp)
  target_include_directories(person_detector_tests PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(person_detector_tests PRIVATE church_director_core ${OpenCV_LIBS})
  church_director_enable_warnings(person_detector_tests)
  add_test(NAME person_detector COMMAND person_detector_tests)
endif()

include(GNUInstallDirs)

add_executable(filter_state_tests filter_state_tests.cpp ../src/obs/filter_state.cpp)
target_link_libraries(filter_state_tests PRIVATE church_director_core)
church_director_enable_warnings(filter_state_tests)
add_test(NAME filter_state COMMAND filter_state_tests)

add_test(NAME plugin_layout COMMAND "${CMAKE_COMMAND}"
  -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
  -DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
  -DTEST_GENERATOR=${CMAKE_GENERATOR}
  -DTEST_LINUX_MODULE_DIR=${CMAKE_INSTALL_LIBDIR}/obs-plugins
  -DTEST_DATA_DIR=${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/church-director
  -P "${CMAKE_CURRENT_SOURCE_DIR}/plugin_layout_test.cmake")

add_test(NAME milestone_docs COMMAND "${CMAKE_COMMAND}"
  -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
  -P "${CMAKE_CURRENT_SOURCE_DIR}/milestone_docs_test.cmake")
