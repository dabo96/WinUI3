# FluentGUI

Una libreria de interfaz grafica híbrida (Immediate Mode GUI + Retained Mode) para C++ inspirada en el Fluent Design System de Microsoft (WinUI3). Similar a Dear ImGui pero con el estilo visual moderno de Fluent Design, y pensada para cubrir desde herramientas de motor grafico hasta aplicaciones de escritorio completas (dashboards, sistemas de gestion, etc.).

*[English version](README_EN.md)*

## Caracteristicas

- **Fluent Design System** — Estilo visual WinUI3 completo (temas light/dark, 6 colores de acento, bordes redondeados, elevation con sombras key+ambient, acrylic/mica, reveal highlight, ripple effects)
- **Immediate Mode API** — Patron Begin/End estilo ImGui, sin arboles de objetos; con ID stack (`PushID`/`PopID`) para evitar colisiones
- **60+ Widgets** — desde los basicos (Button, Label, Checkbox, Slider…) hasta shell de aplicacion (NavigationView, CommandBar), controles de firma (ToggleSwitch, Expander, SplitButton, NumberBox, Flyout, ContentDialog…), feedback (InfoBar, Toast, ProgressRing, Badge, Skeleton) y contenido rico (Hyperlink, AutoSuggestBox, chips, PasswordBox, MarkdownView)
- **Renderizado de alta calidad** — Batch renderer con backends **OpenGL 4.5** y **Vulkan**, formas via SDF (bordes/sombras/reveal en shader), texto MSDF, clipping por scissor
- **Sistema de docking** — Paneles anclables a los bordes o flotantes, con detach a ventana propia
- **Multi-ventana** — Ventanas secundarias con su propio contexto UI
- **Navegacion de aplicacion** — NavigationView responsive, navegacion por paginas con historial (NavFrame), breadcrumbs y titlebar custom
- **Undo/Redo** — Sistema de comandos integrado
- **Atajos de teclado** — Registro de shortcuts con deteccion de conflictos
- **Escalado DPI** — Deteccion automatica por monitor, incluso al mover ventanas entre monitores
- **Accesibilidad** — Focus rings, navegacion por teclado, hooks de Windows UI Automation
- **Internacionalizacion** — Formato por locale, catalogo de strings, layout RTL
- **Dialogos de archivo** — Open/Save/Folder nativos del SO
- **Drag & Drop** — Archivos arrastrados sobre la ventana
- **IME** — Soporte de Input Method Editor para idiomas CJK
- **Clipboard** — Copiar/cortar/pegar en campos de texto (Ctrl+C/X/V)
- **Integracion con motores externos** — Constructor que acepta ventana + contexto GL/Vulkan existente (la UI se dibuja dentro del frame del motor)

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

### Opcion 2: Shell de aplicacion (NavigationView + paginas)

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

int main(int, char**) {
    FluentApp app("Sistema de Inventario", {1280, 800});

    static std::string page = "inicio";
    std::vector<NavItem> nav = {
        {"inicio",     "Inicio",     Icons::House},
        {"inventario", "Inventario", Icons::Box, 3 /* badge */},
        {"informes",   "Informes",   Icons::ChartBar},
    };
    std::vector<NavItem> footer = { {"ajustes", "Ajustes", Icons::Settings} };

    app.root([&](UIBuilder&) {
        BeginHorizontal();
            page = NavigationView("nav", nav, &page, NavDisplayMode::Expanded, footer);
            BeginVertical();
                if      (page == "inicio")     Label("Bienvenido", {}, TypographyStyle::Title);
                else if (page == "inventario") PaginaInventario();
                else if (page == "informes")   PaginaInformes();
                else if (page == "ajustes")    PaginaAjustes();
            EndVertical();
        EndHorizontal();
        RenderToasts(GetContext());   // notificaciones transitorias
    });

    app.run();
    return 0;
}
```

### Opcion 3: Integrar en un motor grafico existente

```cpp
#include "FluentGUI.h"
using namespace FluentUI;

// Tu motor ya tiene una ventana y un contexto OpenGL:
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

> Para un ejemplo completo de integracion (device Vulkan propio + UI dentro del pass del motor), ver `examples/`.

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
| **OpenGL 4.5+** | Backend de render (via GLAD) |
| **Vulkan** | Backend de render alternativo |
| **FreeType** | Carga de fuentes y rasterizacion (bitmap + MSDF) |
| **ZLIB** | Compresion (dependencia de FreeType) |
| **CMake 3.25+** | Sistema de build |

Las dependencias se gestionan con [vcpkg](https://vcpkg.io/):

```bash
vcpkg install sdl3 freetype zlib
```

## Compilacion

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

| Opcion CMake | Default | Descripcion |
|---|---|---|
| `FLUENTUI_BUILD_EXAMPLES` | ON | Compilar las aplicaciones de ejemplo |
| `FLUENTUI_BUILD_TESTS` | OFF | Compilar tests unitarios (Catch2, se descarga automaticamente) |

## Catalogo de widgets

| Categoria | Widgets |
|---|---|
| **Basicos** | Button (+ ButtonSize, con icono), IconButton, Label, IconLabel, LabelWrapped, Separator, Checkbox, RadioButton |
| **Input** | TextInput (undo/redo, IME, seleccion, maxLength, password, callbacks), NumberBox, ComboBox / ComboBoxSearchable / ComboBoxNoLabel, SliderFloat, SliderInt, DragFloat, DragInt, DragFloat3, ColorPicker, RatingControl, ToggleSwitch |
| **Fecha/Hora** | DatePicker, TimePicker, DateTimePicker |
| **Contenedores** | Panel (arrastrable, colapsable), Expander, ScrollView, TabView, Splitter, Grid |
| **Listas** | ListView (seleccion simple/multiple), TreeView, Table (ordenable, columnas redimensionables) |
| **Overlays** | Tooltip, Modal, ContentDialog, ContextMenu, Flyout, MenuFlyout, TeachingTip |
| **Menu/Comandos** | MenuBar, Menu, MenuItem, Toolbar, StatusBar, SplitButton, DropDownButton, CommandBar |
| **Navegacion** | NavigationView, NavFrame (paginas), BreadcrumbBar, TitleBar (window chrome) |
| **Feedback** | InfoBar, Toast (ShowToast/RenderToasts), ProgressBar, ProgressRing, Badge, Skeleton, SkeletonText |
| **Contenido rico** | HyperlinkButton, AutoSuggestBox, TokenizingTextBox (chips), PasswordBox, MarkdownView, Sparkline |
| **Display** | Image (textura GPU) |

Ver [API_REFERENCE.md](API_REFERENCE.md) para firmas y snippets de cada uno.

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
  -> UIContext (estado por ventana; WidgetState unificado)
    -> Renderer (fachada) -> FontSystem (FreeType + MSDF + atlas compartido)
                          -> DrawList / Batcher (batches, clip)
      -> RenderBackend (interfaz abstracta, con capabilities)
        -> OpenGLBackend (OpenGL 4.5)  /  VulkanBackend (Vulkan)
```

Tres capas de render en orden: **Default** -> **Overlay** -> **Tooltip**.

## Modos de integracion

| | `FluentApp("titulo", config)` | `FluentApp(window, gfxCtx)` |
|---|---|---|
| Crea ventana | Si | No |
| Crea contexto GL/Vulkan | Si | No |
| `run()` | Loop completo bloqueante | No usar |
| `beginFrame()`/`endFrame()` | No usar | Tu loop los llama |
| `processEvent()` | No usar (loop interno) | Tu loop pasa eventos |
| Destructor | Destruye ventana + shutdown | Solo destruye UIContext |
| Swap / present | Automatico | Tu motor lo hace |

## Documentacion

- [API_REFERENCE.md](API_REFERENCE.md) — Referencia completa de la API de widgets, con snippets

## CI/CD

El proyecto incluye GitHub Actions con build y tests automatizados (ver `.github/workflows`).

## Licencia

MIT

## Contribuciones

Las contribuciones son bienvenidas. El proyecto usa C++20 y sigue el patron Immediate + Retained Mode GUI.
