# Brief 26 — Build modular y dieta de dependencias (CMake)

**Depende de:** 25 (para que `FLUENTUI_PLATFORM_SDL=OFF` sea real). La dieta de deps (Parte A)
puede hacerse antes, es independiente. **P0** — es el bloqueo práctico de la visión: hoy el dev
del "sistema de inventario" debe instalar el SDK de Vulkan, y el engine que embebe debe linkar SDL.

## Contexto (verificado en `CMakeLists.txt` actual)

- L108–113: `find_package` **REQUIRED** de OpenGL, Vulkan, SDL3, Freetype, ZLIB **y SDL3_image**.
- L131–139: `target_link_libraries(FluentUI PUBLIC OpenGL::GL Vulkan::Vulkan glad SDL3::SDL3
  SDL3_image::SDL3_image Freetype::Freetype ZLIB::ZLIB)` — todo `PUBLIC`, se propaga a cualquier
  consumidor de `FluentUI::FluentUI`.
- **Dependencia muerta:** `SDL3_image` se linka pero no hay ningún uso de `IMG_Load`/`SDL_image`
  en `src/`/`include/` (la carga de imágenes la hace `stb_image` vendorizado).
- No existe ninguna opción `FLUENT_PLATFORM_*` / `FLUENT_BACKEND_*` (solo
  `FLUENTUI_BUILD_EXAMPLES/TESTS`).
- Sí hay `install()` + `install(EXPORT FluentUITargets …)` (L158–189) — buena base de packaging;
  el problema es lo que arrastra.

## Objetivo

Que cada perfil compile y consuma **solo lo que usa**:
- App de empresa: `FLUENTUI_PLATFORM_SDL=ON`, `FLUENTUI_BACKEND_GL=ON`, `BACKEND_VULKAN=OFF` →
  compila sin el SDK de Vulkan.
- Engine embedded: `FLUENTUI_PLATFORM_SDL=OFF`, `BACKEND_VULKAN=ON`, `BACKEND_GL=OFF` → la lib
  resultante no tiene un solo símbolo de SDL/GL.
Y que el linkage no filtre dependencias privadas aguas abajo.

## Parte A — Dieta inmediata (independiente de 25)

1. **Eliminar `SDL3_image`**: quitar el `find_package` (L113) y el link (L136). Verificar que
   nada compila contra él (grep ya dice que no). Si algún ejemplo lo usara, moverlo a `stb_image`.
2. **Revisar ZLIB**: localizar su consumidor real (probablemente Freetype). Si solo lo usa
   Freetype internamente, no debe linkarse `PUBLIC` desde FluentUI — el paquete de Freetype ya
   declara sus propias dependencias.
3. **PUBLIC → PRIVATE por defecto.** Regla: `PUBLIC` solo lo que aparece en headers públicos
   instalados. Auditar: `glad` y `OpenGL::GL` no aparecen en headers públicos → `PRIVATE`.
   `SDL3::SDL3` solo aparece en `SDLPlatform.h` (interno; hoy se instala por error — ver Parte C)
   → `PRIVATE`. `Vulkan::Vulkan`: `VulkanBackend.h` sí expone tipos Vk a quien embebe → `PUBLIC`
   **solo cuando el backend Vulkan está ON**. `Freetype`: verificar que `FontSystem.h` público no
   expone tipos FT → `PRIVATE`.

## Parte B — Opciones de plataforma/backend

```cmake
option(FLUENTUI_PLATFORM_SDL   "Build the SDL3 platform (owned windows/loop)" ON)
option(FLUENTUI_BACKEND_GL     "Build the OpenGL render backend"  ON)
option(FLUENTUI_BACKEND_VULKAN "Build the Vulkan render backend"  ON)
# Guard: al menos un backend ON; PLATFORM_SDL=OFF ⇒ el host provee ventana/superficie/eventos.
```
1. **find_package condicional:**
   ```cmake
   if(FLUENTUI_PLATFORM_SDL)   find_package(SDL3 CONFIG REQUIRED) endif()
   if(FLUENTUI_BACKEND_GL)     find_package(OpenGL REQUIRED)      endif()
   if(FLUENTUI_BACKEND_VULKAN) find_package(Vulkan REQUIRED)      endif()
   ```
2. **Fuentes condicionales:** sacar de la lista fija y añadir por flag:
   `src/Core/SDLPlatform.cpp` + `src/Core/FluentApp.cpp` solo con PLATFORM_SDL (el driver "owned"
   es SDL; el modo embedded no lo usa — si tras el brief 25 FluentApp quedó 100% sobre la
   interfaz, puede compilar siempre y solo SDLPlatform ser condicional; decidir y documentar);
   `OpenGLBackend.cpp` + target `glad` solo con BACKEND_GL;
   `VulkanBackend.cpp` solo con BACKEND_VULKAN.
3. **Defines:**
   `target_compile_definitions(FluentUI PUBLIC
     $<$<BOOL:${FLUENTUI_BACKEND_VULKAN}>:FLUENTUI_HAS_VULKAN>
     $<$<BOOL:${FLUENTUI_BACKEND_GL}>:FLUENTUI_HAS_GL>
     $<$<BOOL:${FLUENTUI_PLATFORM_SDL}>:FLUENTUI_HAS_SDL>)`
   y concentrar los `#ifdef` en UN punto: las factories (`CreateBackendInstance()` /
   `CreateDefaultPlatform()`), no dispersos por el código.
4. **Headers instalados condicionales:** no instalar `VulkanBackend.h`/`EmbeddedShadersVulkan.h`
   con VULKAN=OFF, ni `SDLPlatform.h` nunca (es interno).

## Parte C — Higiene del install

- `install(DIRECTORY include/ …)` copia TODO, incluidos internos (`SDLPlatform.h`,
  `EmbeddedShaders*.h`, `Demo.h`, `stb_image.h`) y el `DX11Backend.h` sin impl (el brief 28 lo
  reubica). Pasar a una **lista explícita de headers públicos**, o mínimo `PATTERN … EXCLUDE`
  para los internos conocidos. La división sana a futuro: `include/FluentUI/` = público,
  `include/core|internal/` = no instalado.
- Generar `FluentUIConfig.cmake` que haga `find_dependency(...)` según las opciones con las que
  se construyó (SDL3 solo si PLATFORM_SDL), para que `find_package(FluentUI)` funcione downstream
  sin sorpresas.

## Aceptación

- **Matriz de builds (cablear en el CI del brief 27):**
  (a) `SDL=ON, GL=ON, VULKAN=OFF` compila y corre la galería **sin** SDK de Vulkan instalado
  (verificable en un contenedor limpio);
  (b) `SDL=OFF, GL=OFF, VULKAN=ON` produce una lib donde `nm`/`dumpbin` no muestra símbolos
  `SDL_` ni `gl[A-Z]`, consumida por el harness embedded del brief 27;
  (c) todo ON = build actual sin regresión.
- `SDL3_image` eliminado. Consumidores de `FluentUI::FluentUI` ya no heredan transitivamente
  glad/SDL/Freetype/ZLIB salvo lo estrictamente público.
- Un proyecto externo mínimo hace `find_package(FluentUI)` y compila/linka con cada variante.

## Fuera de alcance
- Vendorizar SDL vía FetchContent (opción "pilas incluidas" válida; anotar como decisión futura).
  Empaquetado vcpkg/Conan del propio FluentUI.
