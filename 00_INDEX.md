# Migración a renderizado SDF + materiales Fluent — Índice de briefs

Repo: `dabo96/WinUI3` (rama `main`). Librería de UI immediate-mode tipo imgui con
backends **OpenGL** (`src/Core/OpenGLBackend.cpp`) y **Vulkan**
(`src/Core/VulkanBackend.cpp`), renderer en `src/Core/Renderer.cpp`.

## Objetivo global

Hoy todas las formas se rasterizan por **tesselación en CPU** (tablas trigonométricas
`cosTable`/`sinTable`, Path API, AA por "fringe" en `EmitConvexFanWithFringe` /
`StrokePolyline`). El texto ya usa MSDF y no se toca. La meta es mover formas, bordes,
sombras y materiales (Acrylic/Mica) a **shaders SDF + offscreen passes**, conservando
la capa immediate-mode y los dos backends.

## Estado de partida (verificado en el código)

- **Shaders existentes**: `Basic` (color plano), `Text` (R8), `MSDF`, `Image`. Todos
  pass-through. No hay SDF de forma.
- **Vertex layout** (`RenderVertex`): `pos vec2`, `rgba vec4`, `uv vec2`. 3 atributos.
- **GL**: tiene FBOs/render targets (`CreateRenderTarget` etc.) y `SaveState/RestoreState`.
- **Vulkan**: **NO** implementa render targets ni save/restore (caen en stubs no-op).
  Solo `RegisterExternalTexture`.
- **Sombra** (`Renderer::DrawRectShadow`): apila 16–40 rounded-rects expandidos.
- **Acrylic** (`Renderer::DrawRectAcrylic`): falso, 3 rellenos planos; `blurAmount` ignorado.
- **Elevación**: `include/core/Elevation.h` — `Elevation::Z::*` y `Elevation::Params(z)`
  (curva z→blur/offsetY/opacity). Buena capa semántica; se reutiliza tal cual.
- **Tema**: constantes en `FluentColors` + `Style` (global, estilo PushStyleVar).

## Cómo se compilan los shaders (no inventar otro flujo)

- **GL**: strings GLSL crudos en `include/core/EmbeddedShaders.h`, compilados en runtime
  con `OpenGLBackend::CompileShader` / `CreateShaderProgram`. Para añadir un shader: nuevo
  par de strings + nuevo `GLuint program` + entrada en el `switch` de `DrawBatch`.
- **Vulkan**: GLSL fuente en `tools/shaders/*.{vert,frag}`, compilado con
  `tools/shaders/compile.sh` (`glslc -O -mfmt=c`) que **regenera**
  `include/core/EmbeddedShadersVulkan.h` (arrays `uint32_t` + `…Size` en
  `namespace FluentUI::ShadersVK`). Para añadir un shader: crear el `.frag`/`.vert`,
  añadir las líneas `compile`/`emit` en `compile.sh`, correrlo, y crear el pipeline con
  `MakePipeline(frag, layout, topology, wideLines)` en `VulkanBackend::CreatePipelines`.
  **Nunca editar `EmbeddedShadersVulkan.h` a mano.**

## Convención compartida: el buffer de instancias SDF

Todos los briefs de forma (01–04) usan **un quad instanciado** con un buffer de
instancias por-widget, en vez de meter parámetros en `RenderVertex` (que inflaría el
vértice de texto/básico). Layout único, idéntico en GL (atributos con
`glVertexAttribDivisor(...,1)`) y Vulkan (binding con `VK_VERTEX_INPUT_RATE_INSTANCE`):

```c
// Compartido CPU/GPU. std140-friendly: múltiplos de 16 bytes.
struct SDFInstance {
    float cx, cy;            // centro del rect (px lógicos)
    float hx, hy;            // half-size (px) SIN contar el borde ni el AA
    float radius;            // radio de esquina (px)
    float borderWidth;       // grosor de borde (px); 0 = sin borde
    float softness;          // ancho AA (px); típico max(1, dpiScale)
    float mode;              // 0=fill, 1=shadow (brief 03), 2=acrylic-mask (brief 06)
    float fillR, fillG, fillB, fillA;
    float borderR, borderG, borderB, borderA;
    // Reveal (brief 04). Si revealIntensity==0, sin efecto.
    float revealIntensity;   // 0..1
    float _pad0, _pad1, _pad2;
};  // 64 bytes (4 vec4 + 2 vec4 + 1 vec4) → 16 floats exactos sin reveal; con reveal 20 → ver brief 04
```

> El quad por-vértice es un único `vec2 aQuad` en `[-1,1]`. El vertex shader expande:
> `local = aQuad * (halfSize + pad)` con `pad = borderWidth + softness + radius*0` (basta
> `borderWidth + softness + 2`), y emite `vLocal = local`, `gl_Position = proj * vec4(center+local,0,1)`.
> El fragment evalúa `sdRoundBox(vLocal, halfSize, radius)`.

### `sdRoundBox` canónico (idéntico en GL y Vk)

```glsl
float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}
```

### Convenciones de coordenadas
- Proyección ortográfica con origen arriba-izquierda (ya existe; el Y-flip de Vk se hace
  con viewport de altura negativa, la matriz es la misma que GL). Todo en px lógicos.
- `dpiScale` vive en `Renderer` (`GetDPIScale`); el AA debe ser 1 px físico → `softness = max(1, dpiScale)`.
- Blending: premultiplied-over no está asumido; el código actual usa alpha-over estándar
  (`src=GL_SRC_ALPHA, dst=GL_ONE_MINUS_SRC_ALPHA`). Mantenerlo.

## Orden de implementación y dependencias

```
01 (pipeline SDF + fill/border, GL+Vk)
        │
        ├──> 02 (Renderer adopta SDF para rect/rounded; retira tesselado)
        │        │
        │        ├──> 03 (sombra SDF; retira el stacking)
        │        └──> 04 (reveal highlight)
        │
05 (Vulkan render targets + SaveState/RestoreState)   ← independiente, en paralelo
        │
        ├──> 06 (backdrop capture + dual Kawase + Acrylic/Mica; GL ya puede, Vk tras 05)
        │         │
        │         └──> 07 (modelo de material data-driven; pliega todo en SDFInstance)
        │
        └──> 08 (multi-ventana con device/recursos compartidos; parte Vk depende de 05)
                  │
                  └──> 09 (paneles desacoplados / multi-viewport; orquesta el detach)

10 (sistema de movimiento: springs + bezier + tokens + exit-transitions + FLIP + stagger)
        ← CPU-side; encaja mejor tras 07, pero independiente de los backends

11 (calidad de sombras: gaussiana analítica + key/ambient + color tematizado + inset)
        ← depende de 01 y 03 (sustituye el falloff exp del 03)
```

- **01** desbloquea 02/03/04/06/07. Empezar aquí.
- **05** no depende de 01; se puede hacer en paralelo y es prerrequisito de **06 en Vulkan** y
  de **08 (Parte B, Vulkan)**.
- **07** es mayormente CPU-side y se hace al final del bloque visual.
- **08–09** son el bloque de **multi-ventana** (arquitectura, no widgets). 08 hace que las
  ventanas secundarias compartan device + atlases/texturas (su parte Vulkan necesita 05); 09
  completa y generaliza el detach de paneles, que hoy está andamiado solo en GL. Este bloque es
  independiente del bloque visual (01–07) salvo por la dependencia de 05; pueden ir en paralelo.
- **10** es el **sistema de movimiento** (springs interrumpibles, easing bezier, tokens de
  motion, exit-transitions sobre el árbol retenido, FLIP, stagger). Es CPU-side e independiente
  de los backends; conviene tras 07 para declarar transiciones en `FluentMaterial`.
- **11** es **calidad de sombras**: reemplaza el falloff exponencial del brief 03 por la
  gaussiana analítica de caja, añade key+ambient, color de sombra tematizado e inset shadows.
  Depende de 01 y 03.

## Bloque de cierre de visión (post-auditoría del main actualizado)

Tras aplicar 01–24, la auditoría contra la visión ("motor gráfico Y software de empresa con la
misma lib") encontró promesas abiertas, cubiertas por:

```
25 (realizar PlatformBackend: SDLPlatform real + NullPlatform; SDL fuera de FluentApp)  ← P0
        │
26 (build modular: flags PLATFORM/BACKEND, deps PRIVATE, quitar SDL3_image, install limpio) ← P0, Parte A independiente
        │
27 (prueba embedded ejecutable: examples/EmbeddedHost + CI GitHub Actions + golden-image tests) ← P0
28 (higiene de superficie: DX11 a experimental, umbrella único, logging seam, políticas de errores/versionado/threading) ← P1, paralelo
```

Orden recomendado: 25 → 26 → 27, con 28 en paralelo. 21–24 (ID stack, estado unificado,
desglose del Renderer, capabilities) ya están aplicados en el main actual.

## Calidad visual de texto

```
29 (pipeline de texto por tamaño: bitmap con hinting por buckets + umbral, plomería de pxRange,
    shader MSDF de referencia, snapping completo)  ← P0 visual, independiente
```

Diagnóstico cerrado con evidencia (atlas inocente; causa = fase subpíxel sin hinting bajo
minificación + pxRange hardcodeado a 4 vs 12 del atlas + fórmula de shader no estándar). El brief
incluye el test de aceptación automatizable (perfil de tinta por columna).

## Criterio de "hecho" transversal
- Paridad visual GL ↔ Vulkan en cada brief (mismo resultado en ambos backends).
- Sin regresiones en `tests/` (`test_gpu_integration.cpp`, `test_widgets_comprehensive.cpp`).
- Mantener el batching: las instancias SDF se acumulan y se emiten en un solo draw por flush
  (igual que el batch actual de quads).
