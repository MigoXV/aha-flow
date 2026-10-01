qt_add_executable(aha_core_tests engine_config_test.cpp)
target_link_libraries(aha_core_tests PRIVATE aha_core Qt6::Test aha_options)
add_test(NAME engine_config COMMAND aha_core_tests)

qt_add_executable(aha_network_tests engine_client_test.cpp)
target_link_libraries(aha_network_tests PRIVATE aha_network Qt6::Test aha_options)
add_test(NAME engine_client COMMAND aha_network_tests)

add_test(NAME app_startup COMMAND aha-flow --smoke-test)
set_tests_properties(app_startup PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 10)
set_tests_properties(engine_config engine_client PROPERTIES TIMEOUT 20)
