include(FetchContent)

# Keep dependency options scoped: disabling their tests must not disable ours.
function(aha_audio_dependencies)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_CXXLIBS OFF)
    set(BUILD_PROGRAMS OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_DOCS OFF)
    set(WITH_OGG OFF)
    set(INSTALL_MANPAGES OFF)
    set(LIBSAMPLERATE_EXAMPLES OFF)
    set(LIBSAMPLERATE_INSTALL OFF)
    FetchContent_Declare(flac
        URL https://codeload.github.com/xiph/flac/tar.gz/refs/tags/1.5.0
        URL_HASH SHA256=aea54ed186ad07a34750399cb27fc216a2b62d0ffcd6dc2e3064a3518c3146f8
    )
    FetchContent_Declare(samplerate
        URL https://codeload.github.com/libsndfile/libsamplerate/tar.gz/refs/tags/0.2.2
        URL_HASH SHA256=16e881487f184250deb4fcb60432d7556ab12cb58caea71ef23960aec6c0405a
    )
    FetchContent_MakeAvailable(flac samplerate)
    install(FILES "${flac_SOURCE_DIR}/COPYING.Xiph"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/aha-flow/licenses/flac" COMPONENT Runtime)
    install(FILES "${samplerate_SOURCE_DIR}/COPYING"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/aha-flow/licenses/libsamplerate" COMPONENT Runtime)
endfunction()
aha_audio_dependencies()
