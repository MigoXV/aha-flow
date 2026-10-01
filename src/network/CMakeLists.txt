add_library(aha_network STATIC engine_client.cpp engine_client.h proxy.cpp proxy.h realtime_client.cpp realtime_client.h text_correction.cpp text_correction.h)
target_link_libraries(aha_network PUBLIC aha_core Qt6::Network Qt6::WebSockets PRIVATE aha_options)
