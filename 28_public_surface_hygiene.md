# Brief 28 — Higiene de superficie pública: DX11, umbrella único, logging seam, política de errores y versionado

**Depende de:** nada (puede ir en paralelo a 25–27; coordinar la parte de install con la Parte C
del 26). **P1** — no bloquea, pero define la experiencia de adopción de la lib por terceros.

## Contexto (verificado en el `main` actual)

- `include/core/DX11Backend.h`: skeleton **sin `.cpp`** (el header lo admite: *"implementation is
  deferred… intentionally NOT in CMakeLists"*), pero vive en `include/core/` y el
  `install(DIRECTORY include/ …)` lo **instala** → API pública cuyo `Init` da error de link.
- **Dos umbrella headers**: `include/FluentGUI.h` ("Main header… USAGE EXAMPLE") y
  `include/FluentUI/API.h` ("Phase H4: Umbrella header re-exporting the entire public API").
  Dos respuestas a "¿qué incluyo?" es una decisión de API sin tomar.
- **Logging ad hoc**: `VulkanBackend.cpp` define `#define VKDBG(msg) fprintf(stderr, …)` y usa
  `fprintf` directo en ≥4 sitios; un `assert` suelto en `Context.cpp`; "soft-fail" comentado en
  `FluentApp.cpp` (filesystem). No hay forma de que una app capture los diagnósticos de la lib.
- **Versionado**: `project(FluentUI VERSION 1.0.0)` existe, pero ningún documento define qué
  promete (SemVer sobre qué superficie), ni qué headers son públicos vs internos.
- **Threading**: `g_ctx` global + `SetCurrentContext` — límite razonable, no documentado como
  contrato.

## Objetivo

Cuatro entregas independientes y pequeñas: (A) DX11 fuera de la superficie instalada;
(B) un único punto de entrada; (C) un logging seam con política de errores escrita;
(D) contratos documentados (versionado, público/interno, threading).

## Parte A — Reubicar `DX11Backend.h`

Elegir una (recomendada la 1):
1. **Moverlo a `include/experimental/DX11Backend.h`** y excluir `experimental/` del install
   (coordinar con 26-C). El comentario del header ya apunta a "separate branch"; `experimental/`
   cumple lo mismo sin perder el scaffolding escrito.
2. Gatearlo tras `FLUENTUI_BACKEND_DX11` (OFF) que además exija el `.cpp` — solo cuando exista la
   impl.
En ambos casos: nada instalable declara API sin implementación. Añadir un check de CI barato:
"todo header instalado compila solo" (un TU que incluya cada header público → detecta también
headers rotos por include faltante).

## Parte B — Umbrella único

1. Decidir la puerta: **`FluentGUI.h`** (ya es el que documenta el usage y el nombre de marca).
2. `FluentGUI.h` pasa a incluir `FluentUI/API.h` (o absorbe su lista de includes) — una sola
   fuente de verdad de "la API completa".
3. `FluentUI/API.h` se conserva como alias documentado *o* se marca deprecated con un
   `#pragma message`/comentario y se retira en la siguiente minor. Elegir y documentar en README.
4. README/API_REFERENCE: los ejemplos usan solo la puerta elegida.

## Parte C — Logging seam + política de errores

1. **Seam** (`include/core/Log.h` + `src/Core/Log.cpp`):
   ```cpp
   enum class LogLevel { Trace, Debug, Info, Warn, Error };
   using LogFn = void(*)(LogLevel, const char* subsystem, const char* msg, void* user);
   void SetLogHandler(LogFn fn, void* user = nullptr);   // default: stderr con nivel>=Warn
   void SetLogLevel(LogLevel minLevel);
   // interno:
   void LogMsg(LogLevel, const char* subsystem, const char* fmt, ...);
   #define FUI_LOG_WARN(subsys, ...)  ::FluentUI::LogMsg(LogLevel::Warn,  subsys, __VA_ARGS__)
   // … Trace/Debug/Info/Error; Trace/Debug compilables fuera en Release si se quiere.
   ```
2. **Migrar los sitios existentes**: `VKDBG` → `FUI_LOG_DEBUG("vk", …)`; los `fprintf` de
   `VulkanBackend.cpp` (props del GPU, swapchain extent, cmd-buffer olvidado) → Info/Warn/Error
   según el caso; el soft-fail de filesystem en `FluentApp.cpp` → Warn. Búsqueda global de
   `fprintf(stderr` fuera de Log.cpp → 0.
3. **Política de errores escrita** (`docs/ERROR_HANDLING.md`), una página con la regla por clase
   de fallo — propuesta coherente con el código actual:
   - *Errores de programador* (PushID desbalanceado, params inválidos): `assert` en Debug +
     `FUI_LOG_ERROR` y comportamiento definido en Release (ignorar/clamp). Sin excepciones.
   - *Fallos de recursos* (fuente no encontrada, textura inválida): retorno falsy + `Warn`, la UI
     degrada (fuente fallback, placeholder) — nunca crash.
   - *Fallos de device/backend* (device lost, capability ausente): `Error` + camino de
     degradación documentado (p.ej. acrylic→plano vía `Supports`), y un callback opcional
     `SetDeviceErrorHandler` para que el host decida (reintentar/reiniciar backend).
   - La lib **no lanza excepciones** a través de su API (documentarlo; ya es el estilo actual).

## Parte D — Contratos documentados

1. **`docs/VERSIONING.md`** (media página): SemVer; la superficie estable es la que exporta el
   umbrella (Parte B) — todo lo demás (`include/core` interno, `experimental/`) puede cambiar en
   minor. Deprecations: se marcan una minor antes de retirarse. (Coordinar con la separación
   público/interno del install, 26-C: idealmente "lo instalado ≡ lo estable".)
2. **Threading como contrato** (en README + comentario en `Context.h` junto a
   `SetCurrentContext`): un `UIContext` pertenece a un hilo; la construcción de UI no es
   concurrente; multi-ventana = mismo hilo conmutando contextos. Es el modelo actual (`g_ctx`
   global) — declararlo evita que un adoptante lo descubra con un data race.
3. **API_REFERENCE.md**: añadir una sección "Integration modes" con los dos perfiles de la visión
   (owned: FluentApp; embedded: device externo + NullPlatform + `SetFrameCommandBuffer`),
   enlazando al ejemplo `EmbeddedHost` (brief 27) como referencia canónica.

## Aceptación

- El árbol instalado no contiene headers sin implementación ni internos (`DX11Backend.h` fuera o
  gateado; verificado por el check "cada header instalado compila solo" en CI).
- Una sola puerta de entrada documentada; README/ejemplos consistentes con ella.
- `grep -rn "fprintf(stderr" src/ | grep -v Log.cpp` → 0. Una app de prueba instala un
  `SetLogHandler` y captura los diagnósticos de un fallo inducido (p.ej. fuente inexistente).
- `docs/ERROR_HANDLING.md` y `docs/VERSIONING.md` existen y reflejan el comportamiento real;
  el contrato de threading está en README y `Context.h`.

## Fuera de alcance
- Implementar DX11 (solo se reubica). Sistema de deprecation con atributos `[[deprecated]]`
  masivo (hacerlo solo donde ya haya reemplazo). Telemetría/metrics (no aplica a una lib UI).
