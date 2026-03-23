# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**FluentUI** (aka FluentGUI) is an Immediate Mode GUI library (ImGui-style) for C++ that implements Microsoft's Fluent Design System / WinUI3 visual style. It uses a Begin/End pattern API, batch rendering with OpenGL 4.5, and MSDF text rendering. Written in C++20. The project README and comments are in Spanish.

## Build Commands

```bash
# Configure (from repo root)
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"

# Build
cmake --build . --config Release

# The example executable is FluentGUIExample.exe (Windows)
```

**Dependencies** are managed via vcpkg (falls back to `C:/vcpkg/installed/x64-windows/share` if `VCPKG_ROOT` not set): SDL3, OpenGL 4.5+ (via GLAD), FreeType, ZLIB, SDL3_image.

**No automated tests exist yet.** The README lists automated tests as a future improvement.

## Architecture

### Core Pipeline

```
UIContext (singleton, global state)
  -> Renderer (batch management, text/glyph atlas)
    -> RenderBackend (abstract interface)
      -> OpenGLBackend (concrete OpenGL 4.5 impl)
```

- **UIContext** (`include/core/Context.h`): Singleton holding all frame state, layout stack, per-widget state maps (bool/float/int/string/caret), animation maps, focus system, and deferred rendering vectors. Created via `CreateContext(SDL_Window*)`, frame cycle is `NewFrame()` -> widgets -> `Render()`.
- **Renderer** (`include/core/Renderer.h`): Manages draw batches per render layer (Default, Overlay, Tooltip). Handles FreeType font loading, MSDF glyph atlas (2048x2048), text measurement caching, and all draw primitives (DrawRect, DrawText, DrawCircle, etc.).
- **RenderBackend** (`include/core/RenderBackend.h`): Abstract interface decoupling rendering from the graphics API. Enables future Vulkan/DirectX/Metal backends.
- **OpenGLBackend** (`include/core/OpenGLBackend.h`, `src/Core/OpenGLBackend.cpp`): Three shader programs (Basic, Text, MSDF). Extensive GL state caching, pre-allocated VBO/EBO, scissor-based clipping.
- **EmbeddedShaders** (`include/core/EmbeddedShaders.h`): GLSL shader source embedded as C++ string literals (replaced the deleted `shaders/` directory).

### Widget System

Immediate mode API in `include/UI/Widgets.h` (namespace `FluentUI`). Implementations split across 7 files in `src/UI/`:

| File | Widgets |
|---|---|
| BasicWidgets.cpp | Button, Label, Separator, Checkbox, RadioButton, Slider, ProgressBar |
| InputWidgets.cpp | TextInput, ComboBox (with deferred dropdown) |
| ContainerWidgets.cpp | Panel, ScrollView, TabView |
| ListWidgets.cpp | ListView, TreeView |
| MenuWidgets.cpp | MenuBar, Menu, MenuItem |
| OverlayWidgets.cpp | Tooltip, Modal, ContextMenu |
| WidgetHelpers.cpp | Shared utilities, scrollbar drawing, layout calculations |

Widget state is stored in UIContext maps keyed by widget ID (generated via `GenerateId()` hash). Overlays (tooltips, dropdowns, context menus, modals) use deferred rendering to draw on top layers.

### Theming

- `include/Theme/Style.h`: Style data structures (ColorState, TextStyle, Typography, ButtonStyle, PanelStyle, etc.)
- `include/Theme/FluentTheme.h` + `src/Theme/FluentTheme.cpp`: Light/Dark themes, 6 accent colors, `GetDarkFluentStyle()` (default), `GetDefaultFluentStyle()`, `CreateCustomFluentStyle(accent, darkTheme)`

### Layout

Stack-based layout system with `BeginVertical()`/`EndVertical()`, `BeginHorizontal()`/`EndHorizontal()`, constraint-based sizing (Fixed/Fill/Auto via `SetNextConstraints()`).

### Key Performance Patterns

The codebase uses numbered issue markers (e.g., "Issue 1", "Issue 12") in comments referencing specific optimizations:
- Batch state caching to minimize flushes
- Pre-allocated VBO/EBO (~320KB/~60KB)
- GL uniform and state caching
- Text measurement cache (512 entries, frame-based eviction every 120 frames)
- Amortized GC rotation across 12 state maps (one map every 25 frames)
- Reusable ID pool (max 1000)

## Key Conventions

- **Namespace:** All public API is in `FluentUI::`
- **Widget pattern:** Functions return `bool` for interaction (e.g., `Button()` returns true on click), containers use `Begin*()`/`End*()` pairs
- **State access:** `GetContext()` returns the global `UIContext*`; widget state stored in unordered maps by string ID
- **Render layers:** Three layers rendered in order: Default -> Overlay -> Tooltip
- **Shader types:** `ShaderType::Basic`, `ShaderType::Text`, `ShaderType::MSDF`
