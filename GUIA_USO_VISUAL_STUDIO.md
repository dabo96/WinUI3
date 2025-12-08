# Guía de Uso: FluentGUI en Visual Studio

Esta guía explica cómo integrar y usar FluentGUI en proyectos de Visual Studio.

## 📋 Opciones de Integración

Hay dos formas principales de usar FluentGUI en Visual Studio:

### Opción 1: Incluir el Código Fuente (Recomendado para desarrollo)
Incluir los archivos fuente directamente en tu proyecto.

### Opción 2: Compilar como Biblioteca Estática (.lib)
Compilar FluentGUI como una biblioteca estática y linkearla a tu proyecto.

---

## 🔧 Opción 1: Incluir Código Fuente

### Paso 1: Copiar Archivos Necesarios

Copia la siguiente estructura de directorios a tu proyecto:

```
TuProyecto/
├── FluentGUI/              # Carpeta con FluentGUI
│   ├── include/            # Headers públicos (TODO)
│   │   ├── core/
│   │   ├── UI/
│   │   ├── Theme/
│   │   ├── Math/
│   │   └── FluentGUI.h     # Header principal
│   ├── src/                # Código fuente (TODO)
│   │   ├── Core/
│   │   ├── UI/
│   │   └── Theme/
│   ├── shaders/            # Shaders GLSL (TODO)
│   └── assets/             # Assets (fuentes, etc.) (TODO)
└── TuAplicacion/
    └── main.cpp
```

### Paso 2: Configurar Visual Studio

#### 2.1. Agregar Directorio de Include

1. Click derecho en el proyecto → **Properties**
2. **Configuration Properties** → **C/C++** → **General**
3. **Additional Include Directories**: Agregar:
   ```
   $(ProjectDir)FluentGUI/include
   $(ProjectDir)FluentGUI/external/glad/include
   ```

#### 2.2. Agregar Archivos Fuente

1. Click derecho en **Source Files** → **Add** → **Existing Item**
2. Agregar todos los archivos `.cpp` de `FluentGUI/src/`:
   - `Core/Context.cpp`
   - `Core/Renderer.cpp`
   - `Core/Element.cpp`
   - `Core/FontMSDF.cpp`
   - `Core/MSDFGenerator.cpp`
   - `Core/InputState.cpp`
   - `Core/Panel.cpp`
   - `Core/Button.cpp`
   - `UI/Widgets.cpp`
   - `Theme/FluentTheme.cpp`

#### 2.3. Agregar Archivos de Headers (Opcional, para IntelliSense)

1. Click derecho en **Header Files** → **Add** → **Existing Item**
2. Agregar todos los headers de `FluentGUI/include/`

#### 2.4. Agregar GLAD

1. Agregar `FluentGUI/external/glad/src/glad.c` a tus Source Files

#### 2.5. Configurar C++ Standard

1. **Configuration Properties** → **C/C++** → **Language**
2. **C++ Language Standard**: `ISO C++20` o superior

### Paso 3: Configurar Dependencias

#### 3.1. Linkear Bibliotecas

1. **Configuration Properties** → **Linker** → **Input**
2. **Additional Dependencies**: Agregar:
   ```
   opengl32.lib
   SDL3.lib
   freetype.lib
   zlib.lib
   SDL3_image.lib
   ```

Si usas vcpkg, las rutas se configurarán automáticamente. Si no:

#### 3.2. Directorios de Librerías (si no usas vcpkg)

1. **Configuration Properties** → **Linker** → **General**
2. **Additional Library Directories**: Agregar rutas a tus bibliotecas

### Paso 4: Configurar Recursos

#### 4.1. Copiar Shaders y Assets

Copia las carpetas `shaders/` y `assets/` a la carpeta de salida de tu proyecto:

1. **Configuration Properties** → **Build Events** → **Post-Build Event**
2. **Command Line**: Agregar comandos para copiar:
   ```batch
   xcopy /Y /I "$(ProjectDir)FluentGUI\shaders" "$(OutDir)shaders"
   xcopy /Y /I "$(ProjectDir)FluentGUI\assets" "$(OutDir)assets"
   ```

O manualmente copia `shaders/` y `assets/` a la misma carpeta donde está tu `.exe`.

### Paso 5: Ejemplo de Uso

```cpp
#include <SDL3/SDL.h>
#include "FluentGUI.h"  // Header principal

using namespace FluentUI;

int main() {
    // Inicializar SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "Mi Aplicación", 
        1280, 720,
        SDL_WINDOW_OPENGL |     
    );
    
    // Inicializar FluentGUI
    UIContext* ctx = CreateContext(window);
    
    bool running = true;
    SDL_Event e;
    
    while (running) {
        // Procesar eventos
        while (SDL_PollEvent(&e)) {
            ctx->input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        // Nuevo frame
        NewFrame();
        
        // Construir UI
        BeginVertical();
        if (Button("Hola FluentGUI!")) {
            // Click en botón
        }
        Label("Bienvenido a mi aplicación");
        EndVertical();
        
        // Renderizar
        Render();
        SDL_GL_SwapWindow(window);
    }
    
    // Limpiar
    DestroyContext();
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
```

---

## 📦 Opción 2: Biblioteca Estática (.lib)

### Paso 1: Compilar FluentGUI como Biblioteca

#### Usando CMake (Recomendado)

1. Abre **Visual Studio Developer Command Prompt**
2. Navega a la carpeta de FluentGUI
3. Ejecuta:
   ```batch
   mkdir build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

Esto generará `FluentUI.lib` en `build/src/Release/` (o `Debug` según la configuración).

#### Configuración Manual en Visual Studio

1. Crea un nuevo proyecto **Static Library (.lib)** en Visual Studio
2. Agrega todos los archivos fuente de FluentGUI
3. Configura las dependencias (SDL3, FreeType, etc.)
4. Compila para generar `FluentGUI.lib`

### Paso 2: Usar la Biblioteca en tu Proyecto

#### 2.1. Configurar Directorios de Include

1. **Configuration Properties** → **C/C++** → **General**
2. **Additional Include Directories**:
   ```
   RUTA_A_FluentGUI/include
   ```

#### 2.2. Linkear la Biblioteca

1. **Configuration Properties** → **Linker** → **Input**
2. **Additional Dependencies**:
   ```
   FluentUI.lib
   opengl32.lib
   SDL3.lib
   freetype.lib
   zlib.lib
   SDL3_image.lib
   ```

3. **Configuration Properties** → **Linker** → **General**
4. **Additional Library Directories**:
   ```
   RUTA_A_FluentGUI/build/src/Release
   ```

#### 2.3. Copiar Recursos

Igual que en la Opción 1, copia `shaders/` y `assets/` a la carpeta de salida.

---

## 📁 Estructura de Archivos Necesarios

### Headers Públicos (include/)
```
include/
├── FluentGUI.h          # Header principal (incluye todo)
├── core/
│   ├── Context.h
│   ├── Renderer.h
│   ├── InputState.h
│   ├── Button.h
│   ├── Panel.h
│   ├── Element.h
│   ├── Animation.h
│   ├── Elevation.h
│   ├── RippleEffect.h
│   ├── FontMSDF.h
│   └── MSDFGenerator.h
├── UI/
│   ├── Widgets.h
│   └── Layout.h
├── Theme/
│   ├── FluentTheme.h
│   └── Style.h
└── Math/
    ├── Vec2.h
    └── Color.h
```

### Código Fuente (src/)
```
src/
├── Core/
│   ├── Context.cpp
│   ├── Renderer.cpp
│   ├── InputState.cpp
│   ├── Element.cpp
│   ├── Panel.cpp
│   ├── Button.cpp
│   ├── FontMSDF.cpp
│   └── MSDFGenerator.cpp
├── UI/
│   └── Widgets.cpp
└── Theme/
    └── FluentTheme.cpp
```

### Recursos
```
shaders/
├── vertex.glsl
├── fragment.glsl
├── text_vertex.glsl
├── text_fragment.glsl
├── text_vertex_msdf.glsl
└── text_fragment_msdf.glsl

assets/
└── fonts/
    ├── atlas.json
    └── atlas.png
```

---

## ⚙️ Configuración Recomendada en Visual Studio

### Configuración del Proyecto

1. **C++ Language Standard**: `C++20` o superior
2. **Runtime Library**: `Multi-threaded DLL (/MD)` o `Multi-threaded (/MT)` según tus preferencias
3. **Character Set**: `Unicode` o `Multi-byte`

### Dependencias Requeridas

Asegúrate de tener instaladas:

- **SDL3**: Gestión de ventanas y eventos
- **FreeType**: Renderizado de fuentes
- **OpenGL**: Renderizado (incluido en Windows como `opengl32.lib`)
- **GLAD**: Loader de OpenGL (incluido en FluentGUI)

### vcpkg (Recomendado)

Si usas vcpkg, las dependencias se configurarán automáticamente:

```batch
vcpkg install sdl3:x64-windows
vcpkg install freetype:x64-windows
vcpkg install zlib:x64-windows
vcpkg install sdl3-image:x64-windows
```

Luego integra vcpkg en Visual Studio:
```batch
vcpkg integrate install
```

---

## 🐛 Solución de Problemas Comunes

### Error: "Cannot open include file: 'SDL3/SDL.h'"
- Verifica que SDL3 esté instalado y configurado correctamente
- Revisa **Additional Include Directories**

### Error: "Unresolved external symbol"
- Verifica que todas las bibliotecas estén en **Additional Dependencies**
- Asegúrate de que las rutas en **Additional Library Directories** sean correctas

### Error: "Shader files not found"
- Verifica que `shaders/` esté en la misma carpeta que tu `.exe`
- Revisa la configuración de **Post-Build Event**

### Error: "Font atlas not found"
- Verifica que `assets/fonts/` esté en la misma carpeta que tu `.exe`
- Copia la carpeta `assets/` completa

### Error de compilación C++20
- Verifica que tu proyecto use C++20 o superior
- Visual Studio 2019 16.10+ o Visual Studio 2022+ recomendado

---

## 📝 Notas Importantes

1. **Recursos en Tiempo de Ejecución**: Los shaders y assets deben estar accesibles en tiempo de ejecución. Asegúrate de copiarlos a la carpeta de salida.

2. **Configuración Debug vs Release**: Puedes necesitar diferentes configuraciones para Debug y Release. Configura ambas en Visual Studio.

3. **Rutas Absolutas vs Relativas**: Prefiere rutas relativas a `$(ProjectDir)` para mayor portabilidad.

4. **vcpkg Integration**: Si usas vcpkg, asegúrate de ejecutar `vcpkg integrate install` para que Visual Studio encuentre las bibliotecas automáticamente.

---

## ✅ Checklist de Configuración

- [ ] Headers agregados a **Additional Include Directories**
- [ ] Archivos fuente agregados al proyecto (si usas Opción 1)
- [ ] Bibliotecas linkeadas en **Additional Dependencies**
- [ ] GLAD incluido y compilado
- [ ] Carpeta `shaders/` copiada a carpeta de salida
- [ ] Carpeta `assets/` copiada a carpeta de salida
- [ ] C++20 habilitado
- [ ] Dependencias (SDL3, FreeType) instaladas
- [ ] Proyecto compila sin errores
- [ ] Ejecutable encuentra shaders y assets

---

## 🎯 Ejemplo Completo Mínimo

```cpp
#include <SDL3/SDL.h>
#include "FluentGUI.h"

using namespace FluentUI;

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_Window* window = SDL_CreateWindow(
        "Mi App", 800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    
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
        if (Button("Click Me!")) {
            // Clicked!
        }
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

¡Listo! Ahora deberías poder compilar y ejecutar tu aplicación con FluentGUI.

