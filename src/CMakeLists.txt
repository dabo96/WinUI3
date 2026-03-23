add_library(FluentUI
    Core/Context.cpp
    Core/Renderer.cpp
    Core/FontMSDF.cpp
    Core/MSDFGenerator.cpp
    Core/InputState.cpp
    Core/OpenGLBackend.cpp
    Core/UIBuilder.cpp
    Core/FluentApp.cpp
    Core/DockSystem.cpp
    Core/LayoutSerializer.cpp
    Core/FontManager.cpp
    Core/Accessibility.cpp
    Core/FileDialog.cpp
    UI/WidgetHelpers.cpp
    UI/BasicWidgets.cpp
    UI/InputWidgets.cpp
    UI/ContainerWidgets.cpp
    UI/ListWidgets.cpp
    UI/MenuWidgets.cpp
    UI/OverlayWidgets.cpp
    Theme/FluentTheme.cpp)

# Include dirs are set in the parent CMakeLists.txt via BUILD_INTERFACE/INSTALL_INTERFACE
