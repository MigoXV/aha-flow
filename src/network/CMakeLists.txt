add_library(aha_network STATIC engine_client.cpp engine_client.h)
target_link_libraries(aha_network PUBLIC aha_core Qt6::Network PRIVATE aha_options)
