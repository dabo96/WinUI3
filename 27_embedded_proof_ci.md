# Brief 27 — Prueba ejecutable del modo embedded + CI + golden-image tests

**Depende de:** 25 (NullPlatform / core conducible sin SDL) y se cablea con 26 (la matriz de
builds corre aquí). **P0** — el modo embedded es la mitad de la visión y hoy no tiene ninguna
prueba ejecutable; sin CI, se rompe en silencio (ya ocurrió: los stubs que motivaron el brief 05).

## Contexto (verificado)

- `examples/EngineEditor` es una **simulación** de editor (lista de game objects) corriendo
  standalone con `FluentApp` — no embebe nada: no hay device externo ni frame externo.
- El mecanismo embedded existe (`VulkanBackend` en modo shared + `SetFrameCommandBuffer`,
  documentado en `VulkanBackend.h`), pero nada lo compila/ejecuta continuamente.
- No hay `.github/` → **no hay CI**. Con ~47k líneas, 2 backends activos + 1 skeleton, y 2 modos
  de integración, cada push es una apuesta.
- `tests/` tiene buena base unitaria (math, generate_id, serializers, undo, shortcuts,
  widgets_comprehensive, gpu_integration) pero **cero regresión visual** — y el proyecto lleva
  varios refactors de render (SDF, sombras gaussianas, acrylic) validados a ojo.

## Objetivo

Tres redes de seguridad: (A) un **host embedded real** como ejemplo compilado siempre;
(B) **CI en GitHub Actions** con la matriz de builds del brief 26 + tests; (C) **golden-image
tests** del render.

## Parte A — `examples/EmbeddedHost`

Un ejecutable pequeño que demuestra el contrato embedded de punta a punta. Estructura:

1. **El "engine" mínimo:** crea SU propia ventana y device — sin FluentApp. Para no depender de
   otra lib de ventanas en el ejemplo, puede usar SDL directamente (el ejemplo es el host, no la
   lib; deja claro en comentarios que ese SDL es "del engine"). Crea `VkInstance/Device/Swapchain`
   propios (o un GL context propio para la variante GL) y renderiza "su escena": un clear animado
   basta.
2. **Embeber la UI:**
   - Construir el `VulkanSharedContext`/`DeviceHandles` con los handles del engine y crear el
     backend en modo external (la ruta que ya existe en `VulkanBackend`).
   - `CreateContext`/`CreateStandaloneContext` con ese backend; **NullPlatform** (brief 25).
   - Por frame: el engine graba su pass → `backend->SetFrameCommandBuffer(cmd)` →
     `NewFrame(dt)` → construir una UI de prueba (panel con botones, un TextInput, un slider —
     suficiente para ejercitar texto/SDF/input) → `Render()` → el engine hace submit/present.
   - Input: traducir los eventos de la ventana del engine a `UIEvent` y empujarlos al
     `NullPlatform::PushEvent` (o directo a `input.ProcessEvent`). Esto demuestra la ruta de
     input inyectado.
3. **Variante GL** (mismo ejecutable con flag o segundo target): GL context del host compartido
   vía `Init(window, existingGLContext)`.
4. **Modo headless para CI:** con `--headless N`, corre N frames sin present visible (offscreen o
   ventana oculta), inyecta un click sintético sobre el botón de prueba, verifica que el callback
   disparó, y sale con código 0/1. Esto convierte el ejemplo en test.

Este ejemplo pasa a ser **el contrato**: si compila y su modo headless pasa, el camino embedded
está vivo.

## Parte B — CI (GitHub Actions)

`.github/workflows/ci.yml`:

1. **Matriz** (Ubuntu como mínimo; Windows si el tiempo de CI lo permite):
   - `full`: SDL=ON, GL=ON, VULKAN=ON → build + `ctest` + EmbeddedHost `--headless` (con
     lavapipe/SwiftShader como ICD de Vulkan por software, o la variante GL con llvmpipe/Mesa —
     en runners sin GPU usar el rasterizador por software: `mesa-vulkan-drivers` + var
     `VK_ICD_FILENAMES` o `LIBGL_ALWAYS_SOFTWARE=1`).
   - `no-vulkan`: SDL=ON, GL=ON, VULKAN=OFF en un contenedor **sin** SDK de Vulkan → prueba real
     del claim del brief 26.
   - `embedded-only`: SDL=OFF, GL=OFF, VULKAN=ON → build de la lib + check de símbolos
     (`! nm libFluentUI.a | grep -q " SDL_"`).
2. Cache de dependencias (vcpkg/apt) para tiempos razonables. Trigger: push + PR a main.
3. Artefactos: subir los diffs de golden images (Parte C) cuando fallen, para inspección.

## Parte C — Golden-image tests

1. **Harness** (`tests/test_golden_render.cpp`): usando la infra offscreen existente
   (render targets + `ReadPixel`/readback, gated por `Supports(RenderCap::…)`), renderizar
   escenas fijas a un target de tamaño fijo (p.ej. 800×600, dpiScale=1, tema oscuro, seed de
   animaciones congelado — `MotionConfig.reduceMotion=true` para determinismo) y leer los píxeles.
2. **Escenas** (empezar con 4–6): (a) galería de botones/estados; (b) card con sombra gaussiana
   key+ambient; (c) texto MSDF en varios tamaños; (d) flyout con acrylic sobre contenido;
   (e) tabla/list con selección; (f) tema claro de (a).
3. **Comparación con tolerancia**: no igualdad exacta (drivers difieren) — comparar con umbral
   por píxel (ΔRGB ≤ 2–3) y % de píxeles distintos ≤ 0.1%; en CI usar SIEMPRE el rasterizador por
   software (lavapipe/llvmpipe) para que las referencias sean estables entre corridas.
4. **Flujo de actualización**: `--update-golden` regenera las referencias (`tests/golden/*.png`,
   commiteadas al repo); un fallo escribe `actual` + `diff` como artefactos.
5. Ejecutar por backend disponible (GL y Vulkan) — cazará divergencias de paridad entre backends,
   que hoy se validan a ojo.

## Aceptación

- `examples/EmbeddedHost` compila en la matriz, y su `--headless` pasa: UI renderizada dentro del
  frame de un device externo + click sintético manejado, en Vulkan (y variante GL).
- CI verde en: full (tests + embedded headless + golden), no-vulkan (compila sin SDK), 
  embedded-only (sin símbolos SDL). Cualquier PR que rompa el modo embedded o la paridad visual
  **falla en rojo**.
- Golden tests corriendo con ≥4 escenas en GL y Vulkan, referencias commiteadas, flujo de update
  documentado en `tests/README`.

## Fuera de alcance
- CI en macOS (no hay backend Metal). Benchmarks de rendimiento en CI (deseable, aparte).
- Fuzzing de eventos (idea futura sobre NullPlatform::PushEvent).
