# FluentGUI

Una libreria de interfaz grafica inmediata (Immediate Mode GUI) para C++ inspirada en el Fluent Design System de Microsoft (WinUI3). Similar a Dear ImGui pero con el estilo visual moderno de Fluent Design.

*[English version](README_EN.md)*

## Caracteristicas

- **Fluent Design System** — Estilo visual WinUI3 completo (temas light/dark, 6 colores de acento, bordes redondeados, elevation, acrylic, ripple effects)
- **Immediate Mode API** — Patron Begin/End estilo ImGui, sin arboles de objetos
- **30+ Widgets** — Button, Label, TextInput, Checkbox, RadioButton, Slider, ComboBox, ColorPicker, DragFloat/Int, Panel, ScrollView, TabView, ListView, TreeView, Table, Modal, MenuBar, ContextMenu, Tooltip, Splitter, Grid, Toolbar, StatusBar, Image
- **Renderizado de alta calidad** — Batch renderer OpenGL 4.5, texto MSDF, clipping por scissor
- **Sistema de docking** — Paneles anclables a los bordes o flotantes
- **Multi-ventana** — Ventanas secundarias con su propio contexto UI
- **Undo/Redo** — Sistema de comandos integrado
- **Atajos de teclado** — Registro de shortcuts con deteccion de conflictos
- **Escalado DPI** — Deteccion automatica por monitor, incluso al mover ventanas entre monitores
- **Accesibilidad** — Focus rings, navegacion por teclado, hooks de Windows UI Automation
- **Dialogos de archivo** — Open/Save/Folder nativos del SO via SDL3
- **Drag & Drop** — Archivos arrastrados sobre la ventana
- **IME** — Soporte completo de Input Method Editor para idiomas CJK
- **Clipboard** — Copiar/cortar/pegar en campos de texto (Ctrl+C/X/V)
- **Validacion de input** — Limite de longitud configurable en TextInput
- **Font fallback** — Busqueda automatica de glyphs en otras fuentes cargadas
- **Integracion con motores externos** — Constructor que acepta ventana SDL3 + contexto GL existente

## Inicio rapido

### Opcion 1: FluentApp gestiona todo (aplicacion standalone)

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

int main(int, char**) {
    FluentApp app("Mi App", {1280, 720});

    app.root([](UIBuilder& ui) {
        ui.label("Hola FluentGUI!", TypographyStyle::Title);
        if (ui.button("Click")) {
            // accion
        }
    });

    app.run();
    return 0;
}
```

### Opcion 2: Integrar en un motor grafico existente

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

// Tu motor ya tiene una ventana SDL3 y un contexto OpenGL:
SDL_Window* window = /* tu ventana */;
SDL_GLContext gl = SDL_GL_GetCurrentContext();

// FluentUI NO gestiona la ventana, NO la destruye
FluentApp ui(window, gl);

ui.root([&](UIBuilder& b) {
    b.label("Inspector", TypographyStyle::Title);
    b.dragFloat3("Position", myObject.position);
    if (b.button("Delete")) { /* ... */ }
});

// En tu game loop:
while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running = false;
        ui.processEvent(e);     // FluentUI procesa el evento
    }

    renderScene();              // tu renderizado 3D

    ui.beginFrame(dt);          // construye la UI
    ui.endFrame();              // renderiza la UI (NO hace SwapWindow)

    SDL_GL_SwapWindow(window);  // tu motor hace el swap
}
```

### Opcion 3: API de bajo nivel (control total)

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
        if (Button("Click")) { /* accion */ }
        Label("Hola FluentGUI!");
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

## Integracion en tu proyecto

### Estructura de directorios

```
MiProyecto/
├── CMakeLists.txt
├── main.cpp
└── FluentUI/              ← copia o git submodule
    ├── CMakeLists.txt
    ├── include/
    ├── src/
    ├── external/glad/
    ├── assets/fonts/       ← atlas.json + atlas.png (requerido en runtime)
    └── cmake/
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(MiProyecto LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLUENTUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(FluentUI)

add_executable(MiApp main.cpp)
target_link_libraries(MiApp PRIVATE FluentUI::FluentUI)

# Copiar assets al directorio del ejecutable
file(COPY ${FLUENTUI_ASSETS_DIR} DESTINATION ${CMAKE_BINARY_DIR})
```

Tambien funciona con `find_package` despues de instalar:

```cmake
find_package(FluentUI REQUIRED)
target_link_libraries(MiApp PRIVATE FluentUI::FluentUI)
```

## Dependencias

| Dependencia | Uso |
|---|---|
| **SDL3** | Ventanas, input, eventos, dialogos de archivo |
| **OpenGL 4.5+** | Renderizado (via GLAD) |
| **FreeType** | Carga de fuentes y rasterizacion |
| **ZLIB** | Compresion (dependencia de FreeType) |
| **SDL3_image** | Carga de imagenes para texturas |
| **CMake 3.25+** | Sistema de build |

Las dependencias se gestionan con [vcpkg](https://vcpkg.io/):

```bash
vcpkg install sdl3 freetype zlib sdl3-image
```

## Compilacion

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

| Opcion CMake | Default | Descripcion |
|---|---|---|
| `FLUENTUI_BUILD_EXAMPLES` | ON | Compilar la aplicacion de ejemplo |
| `FLUENTUI_BUILD_TESTS` | OFF | Compilar tests unitarios (Catch2, se descarga automaticamente) |

## Catalogo de widgets

| Categoria | Widgets |
|---|---|
| **Basicos** | Button, Label, Separator, Checkbox, RadioButton |
| **Input** | TextInput (undo/redo, IME, seleccion, maxLength), ComboBox, SliderFloat, SliderInt, DragFloat, DragInt, DragFloat3, ColorPicker |
| **Contenedores** | Panel (arrastrable, colapsable), ScrollView, TabView, Splitter, Grid |
| **Listas** | ListView (seleccion simple/multiple), TreeView, Table (ordenable, columnas redimensionables) |
| **Overlays** | Tooltip, Modal, ContextMenu |
| **Menu** | MenuBar, Menu, MenuItem, Toolbar, StatusBar |
| **Display** | ProgressBar, Image (textura GPU) |

## Temas

```cpp
// Tema oscuro (por defecto)
ctx->style = GetDarkFluentStyle();

// Tema claro
ctx->style = GetDefaultFluentStyle();

// Color de acento personalizado
ctx->style = CreateCustomFluentStyle(AccentColor::Purple, true /* dark */);

// Colores de acento: Blue, Green, Purple, Orange, Pink, Teal
```

## Arquitectura

```
FluentApp (shell de aplicacion / wrapper de motor externo)
  -> UIContext (estado global por ventana)
    -> Renderer (batches, atlas de texto/glyphs)
      -> RenderBackend (interfaz abstracta)
        -> OpenGLBackend (implementacion OpenGL 4.5)
```

Tres capas de render en orden: **Default** -> **Overlay** -> **Tooltip**.

## Modos de integracion

| | `FluentApp("titulo", config)` | `FluentApp(window, glCtx)` |
|---|---|---|
| Crea ventana SDL | Si | No |
| Crea contexto GL | Si | No |
| `run()` | Loop completo bloqueante | No usar |
| `beginFrame()`/`endFrame()` | No usar | Tu loop los llama |
| `processEvent()` | No usar (loop interno) | Tu loop pasa eventos |
| Destructor | Destruye ventana + `SDL_Quit()` | Solo destruye UIContext |
| `SDL_GL_SwapWindow` | Automatico | Tu motor lo hace |

## Documentacion

- [API_REFERENCE.md](API_REFERENCE.md) — Referencia completa de la API de widgets
- [CLAUDE.md](CLAUDE.md) — Guia de arquitectura e internos
- [GUIA_USO_VISUAL_STUDIO.md](GUIA_USO_VISUAL_STUDIO.md) — Guia de integracion con Visual Studio

## CI/CD

El proyecto incluye GitHub Actions con build y tests automatizados en Windows, Linux y macOS.

## Licencia

MIT

## Contribuciones

Las contribuciones son bienvenidas. El proyecto usa C++20 y sigue el patron Immediate Mode GUI.
