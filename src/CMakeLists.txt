add_library(FluentUI
    Core/Context.cpp
    Core/Renderer.cpp
    Core/FontMSDF.cpp
    Core/MSDFGenerator.cpp
    Core/InputState.cpp
    UI/WidgetHelpers.cpp
    UI/BasicWidgets.cpp
    UI/InputWidgets.cpp
    UI/ContainerWidgets.cpp
    UI/ListWidgets.cpp
    UI/MenuWidgets.cpp
    UI/OverlayWidgets.cpp
    Theme/FluentTheme.cpp)

target_include_directories(FluentUI PUBLIC ${CMAKE_SOURCE_DIR}/include)
