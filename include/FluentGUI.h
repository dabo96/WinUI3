// FluentGUI.h : Main header file for FluentGUI library
// A Fluent Design System inspired Immediate Mode GUI library

#pragma once

// Core includes
#include "core/Button.h"
#include "core/Context.h"
#include "core/Element.h"
#include "core/InputState.h"
#include "core/Panel.h"
#include "core/Renderer.h"


// UI includes
#include "UI/Layout.h"
#include "UI/Widgets.h"


// Theme includes
#include "Theme/FluentTheme.h"
#include "Theme/Style.h"


// Math includes
#include "Math/Color.h"
#include "Math/Vec2.h"


// Main namespace
namespace FluentUI {
// Convenience function to initialize the library
// Usage: FluentUI::Init(window);
inline UIContext *Init(SDL_Window *window) { return CreateContext(window); }
} // namespace FluentUI
