add_library(aha_storage STATIC audio_cache.cpp audio_cache.h)
target_link_libraries(aha_storage PUBLIC aha_core PRIVATE FLAC aha_options)
