# FluentGUI

Una librería de interfaz gráfica inmediata (Immediate Mode GUI) inspirada en Fluent Design System de Microsoft WinUI3, similar a ImGUI pero con el estilo visual moderno de Fluent.

## 🎯 Objetivo

Crear una librería fácil de usar que permita crear interfaces gráficas modernas con el estilo Fluent Design System, manteniendo la simplicidad de uso de ImGUI.

## 📋 Estado del Proyecto

### ✅ Completado (v1.0)
- ✅ Estructura completa del proyecto (CMake, SDL3, OpenGL)
- ✅ Sistema de contexto y renderizado completo
- ✅ Sistema de input completo (InputState)
- ✅ Estructura de matemáticas (Vec2, Color)
- ✅ Sistema de tema Fluent completo (Light/Dark, colores de acento)
- ✅ Sistema de renderizado completo:
  - Primitivas (rectángulos, círculos, líneas)
  - Batch rendering para eficiencia
  - Renderizado de texto con FreeType
  - Renderizado MSDF de alta calidad (implementación dinámica)
  - Clipping/scissoring
  - Elevation y sombras
  - Efecto Acrylic
  - Ripple effects
- ✅ Sistema de widgets completo:
  - **Básicos:** Button, Label, Separator, Panel, Tooltip
  - **Controles:** TextInput, Checkbox, RadioButton, Slider (Float/Int), ComboBox
  - **Contenedores:** ScrollView, TabView, ListView, TreeView, Modal, MenuBar, ContextMenu
  - **Otros:** ProgressBar
- ✅ Sistema de layout completo (vertical/horizontal, constraints, auto-sizing)
- ✅ Efectos visuales Fluent (sombras, animaciones, ripple effects)
- ✅ API tipo ImGUI (patrón Begin/End)

### 🚧 Mejoras Futuras (Post-v1.0)
- Optimizaciones avanzadas de rendimiento (culling, pooling)
- Tests automatizados
- Ejemplos individuales por widget
- Documentación adicional (guías avanzadas)

## 🏗️ Estructura del Proyecto

```
FluentGUI/
├── include/              # Headers públicos
│   ├── core/            # Núcleo del sistema
│   │   ├── Context.h    # Contexto UI principal
│   │   ├── Renderer.h   # Sistema de renderizado
│   │   ├── Element.h    # Clase base para elementos UI
│   │   ├── Button.h     # Widget Button
│   │   ├── Panel.h      # Widget Panel
│   │   └── InputState.h # Manejo de input
│   ├── UI/              # Widgets y layout
│   │   ├── Widgets.h    # API de widgets
│   │   └── Layout.h      # Sistema de layout
│   ├── Theme/           # Sistema de temas
│   │   ├── Style.h      # Estructura de estilo
│   │   └── FluentTheme.h # Colores y temas Fluent
│   └── Math/            # Utilidades matemáticas
│       ├── Vec2.h
│       └── Color.h
├── src/                  # Implementaciones
├── examples/             # Ejemplos de uso
└── shaders/              # Shaders GLSL
```

## 🚀 Uso Básico

```cpp
#include <SDL3/SDL.h>
#include "core/Context.h"
#include "UI/Widgets.h"

using namespace FluentUI;

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("App", 800, 600, 
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    // Inicializar FluentGUI
    UIContext* ctx = CreateContext(window);
    
    bool running = true;
    SDL_Event e;
    
    while (running) {
        while (SDL_PollEvent(&e)) {
            ctx->input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }
        
        // Nuevo frame
        NewFrame();
        
        // UI
        BeginVertical();
        
        if (Button("Click Me!")) {
            // Handle click
        }
        
        Label("Hello FluentGUI!");
        Separator();
        
        EndVertical();
        
        // Renderizar
        Render();
        SDL_GL_SwapWindow(window);
    }
    
    DestroyContext();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

## 🎨 Características Fluent Design

- **Colores oficiales**: Paleta de colores del Fluent Design System
- **Bordes redondeados**: Radio de 6px por defecto (estilo Fluent)
- **Estados de interacción**: Hover, pressed, disabled
- **Espaciado generoso**: Padding y spacing consistentes

## 📦 Dependencias

- **SDL3**: Gestión de ventanas y eventos
- **OpenGL 4.5+**: Renderizado (via GLAD)
- **CMake 3.25+**: Sistema de construcción
- **vcpkg**: Gestor de paquetes (opcional)

## 🔨 Compilación

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 📚 Documentación

- **API_REFERENCE.md**: Referencia completa de la API
- **GUIA_USO_VISUAL_STUDIO.md**: Guía completa para usar FluentGUI en Visual Studio
- **PLAN_DE_TRABAJO.md**: Plan completo de desarrollo
- **CHANGELOG.md**: Historial de cambios y versiones
- **RELEASE_NOTES_v1.0.md**: Notas detalladas de la versión 1.0

## 🎯 Características Principales

- **Fluent Design System**: Implementación completa del estilo visual de WinUI3
- **Immediate Mode GUI**: API simple tipo ImGUI con patrón Begin/End
- **Renderizado de Alta Calidad**: 
  - Texto MSDF para máxima nitidez
  - Batch rendering para eficiencia
  - Efectos visuales modernos (elevation, acrylic, ripple)
- **Widgets Completos**: Todos los controles y contenedores esenciales
- **Temas**: Soporte para temas Light/Dark con colores de acento configurables
- **Layout Flexible**: Sistema de layout vertical/horizontal con constraints

## 📚 Documentación

- Ver [API_REFERENCE.md](API_REFERENCE.md) para la referencia completa de la API
- Ver [PLAN_DE_TRABAJO.md](PLAN_DE_TRABAJO.md) para el plan de desarrollo

## 📄 Licencia

[Especificar licencia aquí]

## 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor, revisa el plan de trabajo para ver qué se necesita.

