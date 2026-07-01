// FluentGUI.h : Main header file for FluentGUI library
// A Fluent Design System inspired Immediate Mode GUI library
//
// USAGE EXAMPLE (the library owns the window + loop; no platform types needed):
// ============================================================================
// #include "FluentGUI.h"
// using namespace FluentUI;
//
// int main() {
//     FluentApp app("My App", {1280, 720});
//     app.root([](UIBuilder& ui) {
//         ui.button("Click Me!", []{ /* handle click */ });
//         ui.label("Hello FluentGUI!");
//     });
//     app.run();   // blocks until quit; handles events/DPI/present internally
//     return 0;
// }
//
// To embed in an existing engine window instead, construct FluentApp from your
// window/GL handles (as opaque WindowHandle/void*) and drive beginFrame()/
// endFrame() yourself, feeding events via processEvent(UIEvent) — SDL hosts can
// translate with FluentUI::TranslateSDLEvent() from core/SDLPlatform.h.

#pragma once

// Core includes
#include "core/Context.h"
#include "core/InputState.h"
#include "core/Renderer.h"
#include "core/FluentApp.h"
#include "core/UIBuilder.h"
#include "core/DockSystem.h"
#include "core/UndoSystem.h"
#include "core/LayoutSerializer.h"
#include "core/FontManager.h"
#include "core/FileDialog.h"

// UI includes
#include "UI/Layout.h"
#include "UI/Widgets.h"

// Theme includes
#include "Theme/FluentTheme.h"
#include "Theme/Style.h"

// Math includes
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "Math/Rect.h"

// Main namespace
namespace FluentUI {
    // Convenience function to initialize the library
    // Equivalent to CreateContext(window)
    // Usage: UIContext* ctx = FluentUI::Init(window);
    inline UIContext *Init(WindowHandle window) {
        return CreateContext(window);
    }
    
    // Quick access to global context (if you don't want to store the pointer)
    // Usage: UIContext* ctx = FluentUI::GetContext();
    // Note: Returns nullptr if context hasn't been created yet
    // Note: This is already available via ::FluentUI::GetContext() from Context.h
} // namespace FluentUI
