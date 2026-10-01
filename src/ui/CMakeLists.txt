add_library(aha_ui STATIC main_window.cpp main_window.h)
target_link_libraries(aha_ui PUBLIC aha_session Qt6::Widgets PRIVATE aha_options)
