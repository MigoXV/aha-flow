qt_add_executable(aha-flow main.cpp)
target_link_libraries(aha-flow PRIVATE aha_ui aha_options)
target_compile_definitions(aha-flow PRIVATE AHA_FLOW_VERSION="${PROJECT_VERSION}")
set_target_properties(aha-flow PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS aha-flow
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Windows 发布包携带 Qt DLL、平台插件和编译器运行库。
if(WIN32)
    qt_generate_deploy_app_script(TARGET aha-flow OUTPUT_SCRIPT deploy_script)
    install(SCRIPT ${deploy_script})
endif()
