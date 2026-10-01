add_library(aha_ui STATIC main_window.cpp main_window.h settings_window.cpp settings_window.h theme.cpp theme.h widgets.cpp widgets.h)
target_link_libraries(aha_ui PUBLIC aha_session Qt6::Widgets PRIVATE Qt6::Svg aha_options)
qt_add_resources(aha_ui "aha_ui_assets" PREFIX "/aha/ui" BASE "assets" FILES
    assets/vallum/settings.svg
    assets/vallum/collapse.svg
    assets/vallum/close.svg
    assets/vallum/stop.svg
    assets/vallum/mic.svg
    assets/vallum/expand.svg
    assets/vallum/chevron.svg
    assets/vallum/folder.svg
    assets/vallum/toggle-on.svg
    assets/vallum/toggle-off.svg
    assets/abyssus/settings.svg
    assets/abyssus/collapse.svg
    assets/abyssus/close.svg
    assets/abyssus/stop.svg
    assets/abyssus/mic.svg
    assets/abyssus/expand.svg
    assets/abyssus/chevron.svg
    assets/abyssus/folder.svg
    assets/abyssus/toggle-on.svg
    assets/abyssus/toggle-off.svg
)
