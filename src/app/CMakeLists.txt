qt_add_executable(aha-flow main.cpp)
qt_add_resources(aha-flow "aha_app_assets" PREFIX "/aha/app" BASE "assets" FILES
    assets/aha-flow-16.png
    assets/aha-flow-24.png
    assets/aha-flow-32.png
    assets/aha-flow-48.png
    assets/aha-flow-64.png
    assets/aha-flow-128.png
    assets/aha-flow-256.png
    assets/aha-flow-512.png
)
if(WIN32)
    enable_language(RC)
    configure_file(${PROJECT_SOURCE_DIR}/packaging/aha-flow.rc.in aha-flow.rc @ONLY)
    target_sources(aha-flow PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/aha-flow.rc)
endif()
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
    COMPONENT Runtime
)

# Windows 发布包携带 Qt DLL、平台插件和编译器运行库。
if(WIN32)
    qt_generate_deploy_app_script(TARGET aha-flow OUTPUT_SCRIPT deploy_script)
    install(SCRIPT ${deploy_script} COMPONENT Runtime)
endif()

if(UNIX AND NOT APPLE)
    configure_file(${PROJECT_SOURCE_DIR}/packaging/aha-flow.desktop.in aha-flow.desktop @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aha-flow.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Runtime)
    install(FILES assets/aha-flow.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT Runtime)
    foreach(size IN ITEMS 16 24 32 48 64 128 256 512)
        install(FILES assets/aha-flow-${size}.png RENAME aha-flow.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps COMPONENT Runtime)
    endforeach()
endif()
