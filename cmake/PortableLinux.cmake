# 发布包只承诺 Ubuntu 24.04 x86_64；禁止在更新的 glibc 上误构建同名产物。
file(READ /etc/os-release aha_os_release)
if(NOT aha_os_release MATCHES "ID=ubuntu\n" OR
   NOT aha_os_release MATCHES "VERSION_ID=\"24.04\"" OR
   NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    message(FATAL_ERROR "AHA_PORTABLE_LINUX requires an Ubuntu 24.04 x86_64 build environment")
endif()

get_target_property(aha_qmake Qt6::qmake IMPORTED_LOCATION)
execute_process(COMMAND "${aha_qmake}" -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE AHA_QT_PLUGINS OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
set(AHA_SYSTEM_LIBDIR "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
configure_file(${PROJECT_SOURCE_DIR}/packaging/deploy-linux.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/deploy-linux.cmake @ONLY)
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/deploy-linux.cmake COMPONENT Runtime)
install(PROGRAMS ${PROJECT_SOURCE_DIR}/packaging/aha-flow.sh
    DESTINATION . RENAME aha-flow COMPONENT Runtime)
install(FILES ${PROJECT_SOURCE_DIR}/packaging/qt.conf
    DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
