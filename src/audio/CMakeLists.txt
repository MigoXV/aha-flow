add_library(aha_audio STATIC pcm_converter.cpp pcm_converter.h)
target_include_directories(aha_audio PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aha_audio PUBLIC Qt6::Multimedia PRIVATE samplerate aha_options)
