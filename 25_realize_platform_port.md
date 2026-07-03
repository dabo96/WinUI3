# Brief 25 — Realizar el puerto `PlatformBackend` (sacar SDL de FluentApp)

**Depende de:** nada (el brief 20 dejó los tipos listos). **Prerrequisito de:** 26 (el flag
`FLUENT_PLATFORM_NONE` solo tiene sentido si el puerto está realizado) y 27 (el host embedded
conduce el core sin SDL). **P0.**

## Contexto (verificado en el `main` actual)

El brief 20 se aplicó a medias, y la mitad que falta es la clave:
- `include/core/PlatformBackend.h` (51 líneas) declara la interfaz pura correcta
  (`CreateWindowHandle/DestroyWindowHandle/GetFramebufferSize/GetDpiScale/SetWindowTitle/Present/
  PollEvent/WaitEvents/SetClipboardText/GetClipboardText/SetCursor/StartTextInput/StopTextInput/
  GetTicksMs/OpenURL`), pero **nadie la implementa** (`grep ": PlatformBackend"` → vacío).
- `include/core/SDLPlatform.h` (30 líneas) son solo funciones libres de traducción
  (`UIKeyFromScancode`, `TranslateSDLEvent`, `ProcessSDLEvent`); su propio comentario dice
  *"F-c will grow this into a full PlatformBackend implementation"*.
- El loop y toda la plataforma viven **inline en `src/Core/FluentApp.cpp`** (129 refs `SDL_`):
  ventana (`SDL_CreateWindow/DestroyWindow`, 11× `SDL_GetWindowSize`), GL context (14× `SDL_GL*`),
  superficie Vulkan (3× `SDL_Vulkan*`), hit-test de titlebar (12× `SDL_HITTEST*`),
  eventos (`SDL_PollEvent`, 26× `SDL_EVENT*`), DPI (`SDL_GetWindowDisplayScale`), IME
  (`SDL_StartTextInput/StopTextInput`), mouse global, ticks, `SDL_Init/Quit`.

Consecuencia: `FluentApp` fusiona *driver* y *plataforma*; no se puede instanciar un
`NullPlatform` ni conducir el core sin SDL. El puerto es una promesa, no una costura.

## Objetivo

Una clase `SDLPlatform : public PlatformBackend` que encapsule TODO el código SDL hoy inline en
`FluentApp.cpp`, y un `FluentApp` que consuma la plataforma **solo por la interfaz**. Además un
`NullPlatform` mínimo para el modo embedded. Cero cambios de comportamiento visible.

## Archivos

- `include/core/SDLPlatform.h` → pasa de funciones libres a declarar `class SDLPlatform`
  (sigue siendo el ÚNICO header con `<SDL3/SDL.h>`).
- **Nuevo** `src/Core/SDLPlatform.cpp` — la implementación (el código movido de FluentApp).
- **Nuevo** `include/core/NullPlatform.h` (header-only) — no-ops + cola de eventos inyectados.
- `src/Core/FluentApp.cpp` / `include/core/FluentApp.h` — des-SDL-izar; consumir
  `PlatformBackend*`.
- `include/core/PlatformBackend.h` — solo si falta algún método que FluentApp necesita (ver
  Paso 2); mantener la interfaz mínima.

## Paso 1 — `SDLPlatform`

```cpp
// SDLPlatform.h (interno)
class SDLPlatform final : public PlatformBackend {
public:
    bool Initialize(uint32_t sdlInitFlags);   // SDL_Init; llamado por FluentApp una vez
    void Shutdown();                          // SDL_Quit
    // --- PlatformBackend ---
    WindowHandle CreateWindowHandle(const char* title, int w, int h, uint32_t flags) override;
    /* … todos los métodos de la interfaz, moviendo el código actual de FluentApp.cpp … */
private:
    // helpers que hoy están sueltos en SDLPlatform.h se vuelven miembros/estáticos privados:
    // UIKeyFromScancode, TranslateSDLEvent. ProcessSDLEvent desaparece (PollEvent ya traduce).
};
```
- `PollEvent(UIEvent& out)`: envuelve `SDL_PollEvent` + `TranslateSDLEvent`. Eventos que no
  producen `UIEvent` se saltan en bucle interno (no devolver false hasta agotar la cola).
- `WaitEvents(timeoutMs)`: `SDL_WaitEventTimeout` (el idle del brief 10).
- Present: en GL, `SDL_GL_SwapWindow`; nota — el swap es de plataforma+API. Si hoy el swap lo hace
  el backend GL, decidir UNA casa (recomendado: `Present` de la plataforma para GL; en Vulkan es
  no-op porque presenta el swapchain del backend) y documentarlo en el header.

## Paso 2 — Auditar la superficie que FluentApp realmente usa

El grep muestra usos que la interfaz actual NO cubre. Resolver cada uno explícitamente:
- **Hit-test de titlebar** (`SDL_SetWindowHitTest`, 12× `SDL_HITTEST*`): añadir a la interfaz
  `SetWindowHitTest(WindowHandle, HitTestFn)` con un enum neutro
  (`UIHitTest{Normal,Draggable,ResizeTL,…}`) y que SDLPlatform traduzca.
- **Creación de contexto GL / superficie Vulkan** (`SDL_GL_CreateContext`, `SDL_Vulkan_*`): esto
  es el puente plataforma↔render. Opción limpia: `void* CreateGraphicsSurface(WindowHandle,
  GraphicsApi api, void* instance)` en la interfaz (GL: crea+devuelve el GL context; Vulkan:
  crea+devuelve `VkSurfaceKHR`), y los backends la reciben ya creada.
- **Posición de ventana / mouse global / window ID** (drag de paneles detached): añadir
  `GetWindowPosition/SetWindowPosition`, `GetGlobalMousePos`, y que `UIEvent.window` ya porta el
  handle (verificar que el ruteo por window del brief 09 usa `WindowHandle`, no `SDL_WindowID`).
- Lo que solo FluentApp-SDL necesite y no generalice bien puede quedarse como método de
  `SDLPlatform` (downcast interno documentado), pero minimizar.

## Paso 3 — `FluentApp` consume la interfaz

- `FluentApp` posee `std::unique_ptr<PlatformBackend> platform_;` (por defecto crea
  `SDLPlatform`). Constructor alternativo/`setPlatform()` para inyectar otra (tests, embedded).
- Reemplazar cada llamada `SDL_*` de `FluentApp.cpp` por `platform_->…`. Al terminar:
  `grep -c "SDL_" src/Core/FluentApp.cpp` → **0**, y `FluentApp.cpp` ya no incluye
  `SDLPlatform.h` más que en la línea que construye el default (o eso se mueve a una factory
  `CreateDefaultPlatform()` en SDLPlatform.cpp).
- `AppWindow` (ventanas secundarias) idem: sus `SDL_GL*`/hit-test pasan por la interfaz.

## Paso 4 — `NullPlatform`

```cpp
class NullPlatform final : public PlatformBackend {
public:
    void PushEvent(const UIEvent& e) { queue_.push_back(e); }   // el host inyecta
    bool PollEvent(UIEvent& out) override { /* pop de queue_ */ }
    // CreateWindowHandle → devuelve un handle opaco dummy; Present/WaitEvents → no-op;
    // Get/SetClipboardText → buffer interno; GetTicksMs → std::chrono; resto no-op.
};
```
Es la pieza que usa el harness embedded del brief 27.

## Aceptación

- `grep -rn "SDL_" src/Core/FluentApp.cpp` → 0. Único translation unit con SDL:
  `src/Core/SDLPlatform.cpp` (+ su header).
- La app standalone (galería por defecto, `RUN_WIDGET_GALLERY_VULKAN=1` y GL) corre idéntica:
  ventana, resize, DPI, titlebar custom (drag/resize/caption), IME, clipboard, multi-ventana/
  detach — todo pasa ahora por la interfaz sin regresión.
- Un test unitario construye un `NullPlatform`, inyecta eventos `UIEvent` sintéticos
  (mouse move/click sobre un botón) y verifica que el widget reacciona — la UI es conducible sin
  SDL.
- `tests/` existentes pasan.

## Fuera de alcance
- Flags de compilación para excluir SDL del build → **26**. Harness embedded completo → **27**.
- `Win32Platform`/`GLFWPlatform` reales (la costura los habilita; implementarlos es aparte).
