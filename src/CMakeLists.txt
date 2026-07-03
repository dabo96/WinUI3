add_library(FluentUI
    Core/Context.cpp
    Core/Renderer.cpp
    Core/FontSystem.cpp
    Core/FontMSDF.cpp
    Core/MSDFGenerator.cpp
    Core/InputState.cpp
    Core/UIBuilder.cpp
    Core/DockSystem.cpp
    Core/LayoutSerializer.cpp
    Core/FontManager.cpp
    Core/Accessibility.cpp
    Core/FileDialog.cpp
    Core/DragDrop.cpp
    Core/I18n.cpp
    UI/WidgetHelpers.cpp
    UI/BasicWidgets.cpp
    UI/InputWidgets.cpp
    UI/SignatureControls.cpp
    UI/ContainerWidgets.cpp
    UI/LayoutPrimitives.cpp
    UI/ListWidgets.cpp
    UI/CollectionWidgets.cpp
    UI/MenuWidgets.cpp
    UI/OverlayWidgets.cpp
    UI/NavigationWidgets.cpp
    UI/PlotWidgets.cpp
    UI/DateTimeWidgets.cpp
    UI/FeedbackWidgets.cpp
    UI/MarkdownWidgets.cpp

    Demo/DemoWindow.cpp
    Demo/StyleEditor.cpp
    Demo/ItemPicker.cpp
    Theme/FluentTheme.cpp
    Theme/Material.cpp)

# Conditional sources (brief 26-B): each backend / the SDL platform compiles only
# when enabled. The embedded profile (PLATFORM_SDL=OFF) drops the owned driver
# (FluentApp) and SDLPlatform entirely; the host drives CreateStandaloneContext.
if(FLUENTUI_BACKEND_GL)
    target_sources(FluentUI PRIVATE Core/OpenGLBackend.cpp)
endif()
if(FLUENTUI_BACKEND_VULKAN)
    target_sources(FluentUI PRIVATE Core/VulkanBackend.cpp)
endif()
if(FLUENTUI_PLATFORM_SDL)
    target_sources(FluentUI PRIVATE Core/SDLPlatform.cpp Core/FluentApp.cpp)
endif()

# Include dirs are set in the parent CMakeLists.txt via BUILD_INTERFACE/INSTALL_INTERFACE
