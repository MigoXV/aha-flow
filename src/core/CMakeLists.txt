add_library(aha_core STATIC engine_config.cpp engine_config.h settings.cpp settings.h transcription.cpp transcription.h)
target_include_directories(aha_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aha_core PUBLIC Qt6::Core PRIVATE aha_options)
