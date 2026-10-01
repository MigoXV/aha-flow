cmake_minimum_required(VERSION 3.21)

project(aha-flow VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Multimedia WebSockets)
qt_standard_project_setup()

include(CTest)
include(GNUInstallDirs)
include(cmake/AudioDependencies.cmake)

add_library(aha_options INTERFACE)
if(MSVC)
    target_compile_options(aha_options INTERFACE /W4 /utf-8)
else()
    target_compile_options(aha_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

set(CPACK_PACKAGE_NAME aha-flow)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};aha-flow;Runtime;/")
set(CPACK_PACKAGE_FILE_NAME "aha-flow-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
