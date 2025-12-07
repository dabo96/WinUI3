add_library(FluentUI
    Core/Context.cpp
    Core/Renderer.cpp
    "Core/Element.cpp"
    "Core/FontMSDF.cpp"
    UI/Widgets.cpp
    Theme/FluentTheme.cpp
 "Core/InputState.cpp" "Core/Panel.cpp" "Core/Button.cpp")

target_include_directories(FluentUI PUBLIC ${CMAKE_SOURCE_DIR}/include)