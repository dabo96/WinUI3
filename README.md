# FluentGUI

Una librería de interfaz gráfica inmediata (Immediate Mode GUI) inspirada en Fluent Design System de Microsoft WinUI3, similar a ImGUI pero con el estilo visual moderno de Fluent.

## 🎯 Objetivo

Crear una librería fácil de usar que permita crear interfaces gráficas modernas con el estilo Fluent Design System, manteniendo la simplicidad de uso de ImGUI.

## 📋 Estado del Proyecto

### ✅ Completado
- Estructura básica del proyecto (CMake, SDL3, OpenGL)
- Sistema de contexto y renderizado básico
- Sistema de input (InputState)
- Estructura de matemáticas (Vec2, Color)
- Estructura de tema básica (Style)
- Sistema de widgets básico (Button, Panel, Label, Separator)
- Sistema de layout vertical/horizontal
- Tema Fluent básico con colores oficiales

### 🚧 En Progreso
- Sistema de renderizado mejorado (primitivas, batch rendering)
- Renderizado de texto

### 📝 Pendiente
- Widgets avanzados (TextInput, Slider, Checkbox, etc.)
- Sistema de layout avanzado (constraints, auto-sizing)
- Efectos visuales Fluent (sombras, animaciones, ripple effects)
- Renderizado de texto con fuentes
- Optimizaciones de rendimiento

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

Ver [PLAN_DE_TRABAJO.md](PLAN_DE_TRABAJO.md) para el plan completo de desarrollo.

## 🎯 Próximos Pasos

1. Implementar renderizado de texto
2. Agregar más widgets (TextInput, Slider, etc.)
3. Implementar sistema de animaciones
4. Optimizar rendimiento con batch rendering
5. Agregar más ejemplos y documentación

## 📄 Licencia

[Especificar licencia aquí]

## 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor, revisa el plan de trabajo para ver qué se necesita.

