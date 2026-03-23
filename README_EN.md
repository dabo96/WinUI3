# FluentGUI

An Immediate Mode GUI library for C++ inspired by Microsoft's Fluent Design System (WinUI3). Similar to Dear ImGui but with the modern visual style of Fluent Design.

*[Version en espanol](README.md)*

## Features

- **Fluent Design System** — Full WinUI3 visual style (light/dark themes, 6 accent colors, rounded corners, elevation, acrylic, ripple effects)
- **Immediate Mode API** — Simple ImGui-style Begin/End pattern, no object trees to manage
- **30+ Widgets** — Button, Label, TextInput, Checkbox, RadioButton, Slider, ComboBox, ColorPicker, DragFloat/Int, Panel, ScrollView, TabView, ListView, TreeView, Table, Modal, MenuBar, ContextMenu, Tooltip, Splitter, Grid, Toolbar, StatusBar, Image
- **High-Quality Rendering** — OpenGL 4.5 batch renderer, MSDF text rendering, scissor-based clipping
- **Docking System** — Dock panels to edges or float them freely
- **Multi-Window** — Secondary windows with their own UI context
- **Undo/Redo** — Built-in command pattern undo system
- **Keyboard Shortcuts** — Shortcut registry with conflict detection
- **DPI Scaling** — Automatic per-monitor detection, updates when windows move between displays
- **Accessibility** — Focus rings, keyboard navigation, Windows UI Automation hooks
- **File Dialogs** — Native open/save/folder dialogs via SDL3
- **File Drag & Drop** — Handle files dropped onto the window
- **IME Support** — Full Input Method Editor composition for CJK languages
- **Clipboard** — Copy/cut/paste in text fields (Ctrl+C/X/V)
- **Input Validation** — Configurable max length on TextInput fields
- **Font Fallback** — Automatic glyph search across all loaded fonts
- **Engine Integration** — Constructor that accepts an external SDL3 window + GL context

## Quick Start

### Option 1: FluentApp manages everything (standalone application)

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

int main(int, char**) {
    FluentApp app("My App", {1280, 720});

    app.root([](UIBuilder& ui) {
        ui.label("Hello FluentGUI!", TypographyStyle::Title);
        if (ui.button("Click")) {
            // action
        }
    });

    app.run();
    return 0;
}
```

### Option 2: Integrate into an existing game engine

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

// Your engine already has an SDL3 window and OpenGL context:
SDL_Window* window = /* your window */;
SDL_GLContext gl = SDL_GL_GetCurrentContext();

// FluentUI does NOT manage the window, does NOT destroy it
FluentApp ui(window, gl);

ui.root([&](UIBuilder& b) {
    b.label("Inspector", TypographyStyle::Title);
    b.dragFloat3("Position", myObject.position);
    if (b.button("Delete")) { /* ... */ }
});

// In your game loop:
while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running = false;
        ui.processEvent(e);     // FluentUI processes the event
    }

    renderScene();              // your 3D rendering

    ui.beginFrame(dt);          // builds the UI
    ui.endFrame();              // renders the UI (does NOT call SwapWindow)

    SDL_GL_SwapWindow(window);  // your engine does the swap
}
```

### Option 3: Low-level API (full control)

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("App", 800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    UIContext* ctx = CreateContext(window);
    ctx->style = GetDarkFluentStyle();
    SDL_StartTextInput(window);

    bool running = true;
    uint64_t lastTime = SDL_GetTicks();

    while (running) {
        uint64_t now = SDL_GetTicks();
        float dt = std::min((now - lastTime) / 1000.0f, 0.1f);
        lastTime = now;

        ctx->input.Update(window);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                int w, h; SDL_GetWindowSize(window, &w, &h);
                ctx->renderer.SetViewport(w, h);
            }
            else ctx->input.ProcessEvent(e);
        }

        NewFrame(dt);
        BeginVertical();
        if (Button("Click")) { /* action */ }
        Label("Hello FluentGUI!");
        EndVertical();
        RenderDeferredDropdowns();
        Render();
        SDL_GL_SwapWindow(window);
    }

    SDL_StopTextInput(window);
    DestroyContext();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

## Project Integration

### Directory structure

```
MyProject/
├── CMakeLists.txt
├── main.cpp
└── FluentUI/              ← copy or git submodule
    ├── CMakeLists.txt
    ├── include/
    ├── src/
    ├── external/glad/
    ├── assets/fonts/       ← atlas.json + atlas.png (required at runtime)
    └── cmake/
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyProject LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLUENTUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(FluentUI)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE FluentUI::FluentUI)

# Copy assets to build directory
file(COPY ${FLUENTUI_ASSETS_DIR} DESTINATION ${CMAKE_BINARY_DIR})
```

Also works with `find_package` after installing:

```cmake
find_package(FluentUI REQUIRED)
target_link_libraries(MyApp PRIVATE FluentUI::FluentUI)
```

## Dependencies

| Dependency | Purpose |
|---|---|
| **SDL3** | Windows, input, events, file dialogs |
| **OpenGL 4.5+** | Rendering (via GLAD loader) |
| **FreeType** | Font loading and glyph rasterization |
| **ZLIB** | Compression (FreeType dependency) |
| **SDL3_image** | Image loading for textures |
| **CMake 3.25+** | Build system |

Dependencies are managed via [vcpkg](https://vcpkg.io/):

```bash
vcpkg install sdl3 freetype zlib sdl3-image
```

## Building

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

| CMake Option | Default | Description |
|---|---|---|
| `FLUENTUI_BUILD_EXAMPLES` | ON | Build the example application |
| `FLUENTUI_BUILD_TESTS` | OFF | Build unit tests (Catch2, fetched automatically) |

## Widget Gallery

| Category | Widgets |
|---|---|
| **Basic** | Button, Label, Separator, Checkbox, RadioButton |
| **Input** | TextInput (undo/redo, IME, selection, maxLength), ComboBox, SliderFloat, SliderInt, DragFloat, DragInt, DragFloat3, ColorPicker |
| **Containers** | Panel (draggable, collapsible), ScrollView, TabView, Splitter, Grid |
| **Lists** | ListView (single/multi-select), TreeView, Table (sortable, resizable columns) |
| **Overlays** | Tooltip, Modal, ContextMenu |
| **Menu** | MenuBar, Menu, MenuItem, Toolbar, StatusBar |
| **Display** | ProgressBar, Image (GPU texture) |

## Theming

```cpp
// Dark theme (default)
ctx->style = GetDarkFluentStyle();

// Light theme
ctx->style = GetDefaultFluentStyle();

// Custom accent color
ctx->style = CreateCustomFluentStyle(AccentColor::Purple, true /* dark */);

// Available accent colors: Blue, Green, Purple, Orange, Pink, Teal
```

## Architecture

```
FluentApp (application shell / external engine wrapper)
  -> UIContext (per-window global state)
    -> Renderer (batch management, text/glyph atlas)
      -> RenderBackend (abstract interface)
        -> OpenGLBackend (OpenGL 4.5 implementation)
```

Three render layers drawn in order: **Default** -> **Overlay** -> **Tooltip**.

## Integration Modes

| | `FluentApp("title", config)` | `FluentApp(window, glCtx)` |
|---|---|---|
| Creates SDL window | Yes | No |
| Creates GL context | Yes | No |
| `run()` | Blocking main loop | Do not use |
| `beginFrame()`/`endFrame()` | Do not use | Your loop calls these |
| `processEvent()` | Do not use (internal loop) | Your loop passes events |
| Destructor | Destroys window + `SDL_Quit()` | Only destroys UIContext |
| `SDL_GL_SwapWindow` | Automatic | Your engine calls it |

## Documentation

- [API_REFERENCE.md](API_REFERENCE.md) — Complete widget API reference
- [CLAUDE.md](CLAUDE.md) — Architecture and internals guide
- [GUIA_USO_VISUAL_STUDIO.md](GUIA_USO_VISUAL_STUDIO.md) — Visual Studio integration guide (Spanish)

## CI/CD

The project includes GitHub Actions with automated build and tests on Windows, Linux, and macOS.

## License

MIT

## Contributing

Contributions are welcome! The project uses C++20 and follows an immediate-mode GUI pattern.
