add_library(FluentUI
    Core/Context.cpp
    Core/Renderer.cpp
    Core/FontMSDF.cpp
    Core/MSDFGenerator.cpp
    Core/InputState.cpp
    Core/OpenGLBackend.cpp
    Core/VulkanBackend.cpp
    Core/UIBuilder.cpp
    Core/FluentApp.cpp
    Core/DockSystem.cpp
    Core/LayoutSerializer.cpp
    Core/FontManager.cpp
    Core/Accessibility.cpp
    Core/FileDialog.cpp
    Core/DragDrop.cpp
    UI/WidgetHelpers.cpp
    UI/BasicWidgets.cpp
    UI/InputWidgets.cpp
    UI/ContainerWidgets.cpp
    UI/LayoutPrimitives.cpp
    UI/ListWidgets.cpp
    UI/CollectionWidgets.cpp
    UI/MenuWidgets.cpp
    UI/OverlayWidgets.cpp
    UI/PlotWidgets.cpp
    UI/DateTimeWidgets.cpp
    UI/FeedbackWidgets.cpp

    Demo/DemoWindow.cpp
    Demo/StyleEditor.cpp
    Demo/ItemPicker.cpp
    Theme/FluentTheme.cpp
    Theme/Material.cpp)

# Include dirs are set in the parent CMakeLists.txt via BUILD_INTERFACE/INSTALL_INTERFACE
