#pragma once

// Phase H4: Umbrella header re-exporting the entire FluentUI public API.
// Users may include just this file to access every documented symbol.

#include "Math/Color.h"
#include "Math/Vec2.h"

#include "Theme/Style.h"
#include "Theme/FluentTheme.h"

#include "UI/Layout.h"
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"

#include "core/Context.h"
#include "core/Renderer.h"
#include "core/InputState.h"
#include "core/UIBuilder.h"
#include "core/FluentApp.h"
#include "core/Animation.h"
#include "core/RippleEffect.h"
#include "core/DockSystem.h"
#include "core/LayoutSerializer.h"
#include "core/Accessibility.h"
#include "core/FontManager.h"
#include "core/FontMSDF.h"
#include "core/FileDialog.h"

// Phase D
#include "core/DragDrop.h"
// Phase H1-H3
#include "core/Demo.h"
