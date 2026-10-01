qt_add_executable(aha_core_tests engine_config_test.cpp)
target_link_libraries(aha_core_tests PRIVATE aha_core Qt6::Test aha_options)
add_test(NAME engine_config COMMAND aha_core_tests)

qt_add_executable(aha_network_tests engine_client_test.cpp)
target_link_libraries(aha_network_tests PRIVATE aha_network Qt6::Test aha_options)
add_test(NAME engine_client COMMAND aha_network_tests)

add_test(NAME app_startup COMMAND aha-flow --smoke-test)
set_tests_properties(app_startup PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 10)
set_tests_properties(engine_config engine_client PROPERTIES TIMEOUT 20)

qt_add_executable(aha_behavior_tests behavior_test.cpp)
target_link_libraries(aha_behavior_tests PRIVATE aha_network aha_audio Qt6::Test aha_options)
target_compile_definitions(aha_behavior_tests PRIVATE AHA_FLOW_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME behavior COMMAND aha_behavior_tests)

qt_add_executable(aha_cache_tests cache_test.cpp)
target_link_libraries(aha_cache_tests PRIVATE aha_storage FLAC Qt6::Test aha_options)
add_test(NAME audio_cache COMMAND aha_cache_tests)

qt_add_executable(aha_session_tests session_test.cpp)
target_link_libraries(aha_session_tests PRIVATE aha_session Qt6::Test aha_options)
add_test(NAME session_lifecycle COMMAND aha_session_tests)

qt_add_executable(aha_ui_tests ui_test.cpp)
target_link_libraries(aha_ui_tests PRIVATE aha_ui Qt6::Test aha_options)
add_test(NAME ui_behavior COMMAND aha_ui_tests)
set_tests_properties(ui_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
set_tests_properties(behavior audio_cache session_lifecycle PROPERTIES TIMEOUT 30)

qt_add_executable(aha_correction_tests correction_test.cpp)
target_link_libraries(aha_correction_tests PRIVATE aha_network Qt6::Test aha_options)
add_test(NAME text_correction COMMAND aha_correction_tests)
set_tests_properties(text_correction PROPERTIES TIMEOUT 30)
