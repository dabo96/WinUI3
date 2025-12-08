# Integración Rápida - FluentGUI

Guía rápida para integrar FluentGUI en un proyecto nuevo.

## 🚀 Integración Rápida (5 pasos)

### 1. Copiar Archivos
Copia la carpeta `include/` y `src/` de FluentGUI a tu proyecto.

### 2. Configurar Includes
En Visual Studio:
- **Properties** → **C/C++** → **General** → **Additional Include Directories**
- Agregar: `$(ProjectDir)FluentGUI/include`

### 3. Agregar Archivos Fuente
Agregar todos los `.cpp` de `FluentGUI/src/` a tu proyecto:
- `Core/*.cpp`
- `UI/*.cpp`
- `Theme/*.cpp`
- `external/glad/src/glad.c`

### 4. Linkear Bibliotecas
- **Properties** → **Linker** → **Input** → **Additional Dependencies**
- Agregar: `opengl32.lib`, `SDL3.lib`, `freetype.lib`, `zlib.lib`, `SDL3_image.lib`

### 5. Copiar Recursos
Copia `shaders/` y `assets/` a la carpeta donde se genera tu `.exe`

## 📝 Código Mínimo

```cpp
#include <SDL3/SDL.h>
#include "FluentGUI.h"

using namespace FluentUI;

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("App", 800, 600, 
                                          SDL_WINDOW_OPENGL);
    UIContext* ctx = CreateContext(window);
    
    bool running = true;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            ctx->input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }
        NewFrame();
        BeginVertical();
        Button("Hello!");
        EndVertical();
        Render();
        SDL_GL_SwapWindow(window);
    }
    DestroyContext();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

## 📦 Requisitos

- **C++20** o superior
- **SDL3** (vcpkg: `vcpkg install sdl3:x64-windows`)
- **FreeType** (vcpkg: `vcpkg install freetype:x64-windows`)
- **OpenGL 4.5+**

Para más detalles, ver [GUIA_USO_VISUAL_STUDIO.md](GUIA_USO_VISUAL_STUDIO.md)

