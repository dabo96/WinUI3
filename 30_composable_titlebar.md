# Brief 30 — TitleBar componible (editable) con zonas de arrastre automáticas

**Depende de:** la TitleBar actual (brief 13, aplicada) y la infra de items interactivos
(`SetLastItem`/`focusableWidgets`, ya en el código). **Lee primero:** el `TitleBar` actual en
`src/UI/NavigationWidgets.cpp` y `TitleBarHitRegions` en `include/core/Context.h`.

## Contexto (verificado en el `main` actual)

- `TitleBar(id, title, icon, centerContent)` (`NavigationWidgets.cpp`) es de **layout fijo**:
  icono+título a la izquierda, `centerContent()` centrado y clipeado, caption buttons a la
  derecha. Publica en `ctx->titleBarHit` (`TitleBarHitRegions{ caption, exclusions[],
  resizeBorder, resizable }`) el rect arrastrable (toda la barra) y las exclusiones (los caption
  buttons + **toda** la `centerRect`). El callback de hit-test de `SDLPlatform` lee eso.
- Limitaciones: solo se pueden meter widgets en el centro (vía callback); la `centerRect` entera
  queda excluida del arrastre aunque esté vacía; no se controla altura; no hay slots izq/der; el
  layout es rígido.
- Infra reutilizable: cada widget interactivo hace `focusableWidgets.push_back(id)` y publica su
  bbox vía `SetLastItem(id, bboxMin, bboxMax, hovered, active, focused, ...)` →
  `ctx->lastItem`. Los no interactivos (Label, icono) **no** son focusables. `GetPlatform(ctx)`
  expone `Minimize/Maximize/Restore/RequestWindowClose/IsWindowMaximized`.

## Objetivo

Una TitleBar **componible** (Begin/End) que albergue cualquier widget, con **zonas de arrastre
calculadas automáticamente** (todo arrastrable salvo los widgets interactivos), caption buttons
automáticos, altura configurable y slots opcionales. Debe ser **más fácil que WinUI3**: sin
`SetTitleBar`, sin registrar regiones non-client a mano. El `TitleBar(...)` actual se conserva
como atajo, reimplementado sobre lo nuevo (compatibilidad).

## Diseño

### API

`include/UI/NavigationWidgets.h`:
```cpp
struct TitleBarConfig {
    float height = 40.0f;              // 0 = auto (alto del contenido + padding)
    bool  captionButtons = true;       // dibuja min/max/close a la derecha, auto-excluidos
    bool  doubleClickMaximize = true;  // doble-clic en zona arrastrable alterna maximizar
    float resizeBorder = 6.0f;         // grosor de bordes de resize (0 si maximizada)
};

// Begin: fija la región de la barra, dibuja el fondo, reserva el espacio de los caption
// buttons, arranca la CAPTURA de exclusiones de arrastre y coloca un layout horizontal
// centrado verticalmente. Devuelve el resultado de los caption buttons.
TitleBarResult BeginTitleBar(const std::string& id, const TitleBarConfig& cfg = {});
void EndTitleBar();

// Espaciador flexible: empuja el contenido siguiente hacia la derecha (para el patrón
// izquierda | spacer | centro | spacer | derecha). Implementar como avance del cursor.
void TitleBarSpacer(float minWidth = 0.0f);

// Escapes manuales para casos que el heurístico no cubre:
void TitleBarDragExclude(const Rect& r);  // fuerza NO-arrastre en r (contenido custom interactivo)
void TitleBarDragRegion(const Rect& r);   // fuerza arrastre en r (anula una exclusión)
```

### El corazón: captura automática de exclusiones

Añadir a `UIContext` un puntero de captura activo solo dentro del scope de la titlebar:
```cpp
// en UIContext:
struct TitleBarCapture {
    bool active = false;
    size_t focusStart = 0;                         // focusableWidgets.size() al abrir
    std::vector<std::pair<uint32_t, Rect>> items;  // (id, bbox) de cada SetLastItem en scope
    std::vector<Rect> manualExclude, manualDrag;   // de TitleBarDragExclude/Region
} titleBarCapture;
```

1. **`BeginTitleBar`**: `titleBarCapture.active = true; focusStart = focusableWidgets.size();
   items.clear();` etc.
2. **Hook en `SetLastItem`** (`WidgetHelpers.cpp`): al final, si
   `ctx->titleBarCapture.active`, `ctx->titleBarCapture.items.push_back({id,
   Rect(bboxMin, bboxMax - bboxMin)})`. Una línea; sin coste fuera de la titlebar.
3. **`EndTitleBar`**: construir las exclusiones =
   - por cada `(id, rect)` en `items` **cuyo id esté** en
     `focusableWidgets[focusStart .. end]` (⇒ el widget es interactivo: botón/input/toggle/…;
     los Label/iconos no son focusables y **quedan arrastrables**),
   - más `manualExclude`,
   - más el rect de los caption buttons,
   - menos `manualDrag` (restar/anular).
   Publicar en `ctx->titleBarHit`: `caption = <toda la barra>`, `exclusions = <lo anterior>`,
   `resizeBorder = maximized ? 0 : cfg.resizeBorder`, `active = true`. Cerrar la captura.

> Este es el punto que lo hace más simple que WinUI3: el usuario mete widgets normales y las
> regiones non-client salen de que el widget sea focusable. Además, a diferencia del `centerRect`
> actual, **el espacio vacío entre widgets sigue siendo arrastrable** (solo se excluye el bbox real
> de cada control interactivo).

### Layout interno

- `BeginTitleBar` fija `barPos = cursorPos`, `barW = viewport.x - barPos.x`, `barH =
  cfg.height>0 ? S(cfg.height) : auto`. Dibuja el fondo (reusar `AdjustContainerBackground`).
- Reserva a la derecha el ancho de los caption buttons (si `captionButtons`) y abre un **layout
  horizontal** con el cursor centrado verticalmente en la barra (`cursorPos.y = barPos.y +
  (barH - itemH)/2`), acotado a `[barPos.x + pad, capX - pad]`. Reusar el sistema de layout
  horizontal existente; los widgets se alinean solos.
- `TitleBarSpacer()` = avanzar el cursor hasta consumir el espacio flexible (si hay varios
  spacers, repartir; MVP: un spacer empuja el resto a la derecha).
- `EndTitleBar`: dibujar los caption buttons (extraer la lambda `capBtn` actual a un helper
  reutilizable), cablearlos al puerto de plataforma (igual que hoy), fijar su rect como
  exclusión, restaurar el cursor y `AdvanceCursor(barSize)`.

### Caption buttons y comportamiento de ventana

- Reutilizar la lambda `capBtn` actual (min/max/close, hover, glifos) como helper compartido
  entre `BeginTitleBar` y el `TitleBar` legacy.
- `doubleClickMaximize`: en `EndTitleBar`, si hubo doble-clic dentro del `caption` pero **fuera**
  de las exclusiones, alternar `Maximize`/`Restore` por el puerto. (Detección de doble-clic: usar
  el timing de input existente, o `ctx->input` si ya expone doble-clic.)
- Sin margen de resize cuando la ventana está maximizada (ya se hace).

### Compatibilidad

Reimplementar el `TitleBar(id, title, icon, centerContent)` actual **sobre** Begin/End:
```cpp
TitleBarResult TitleBar(id, title, icon, centerContent) {
    auto r = BeginTitleBar(id, {});
    if (icon) IconLabel(icon, title); else if(!title.empty()) Label(title);
    if (centerContent) { TitleBarSpacer(); /* región central */ centerContent(); TitleBarSpacer(); }
    EndTitleBar();
    return r;
}
```
Mismo comportamiento visible que hoy, pero ahora el hueco vacío del centro es arrastrable.

### Ventanas secundarias

`BeginTitleBar` debe funcionar en las ventanas flotantes/detached (brief 09): publica en el
`titleBarHit` del contexto actual (`GetContext()`), que ya es por-ventana. Verificar que
`AppWindow` instala el mismo hit-test (o exponer `AppConfig::useCustomTitleBar` equivalente por
ventana).

## Aceptación

- Componer una titlebar con: `IconLabel` (logo+título), `TitleBarSpacer`, `AutoSuggestBox`
  (buscador), `TitleBarSpacer`, `IconButton` + `SplitButton`. En ella:
  - Arrastrar la ventana funciona desde el título, el fondo y el **espacio vacío entre widgets**.
  - **No** inicia arrastre al interactuar con el buscador, el IconButton ni el SplitButton
    (auto-excluidos). El menú del SplitButton se abre normal.
  - Los caption buttons min/max/close funcionan; doble-clic en zona arrastrable maximiza/restaura.
  - `height` configurable respetado; sin borde de resize al maximizar.
- El `TitleBar(id,title,icon,centerContent)` legacy sigue idéntico visualmente, pero su centro
  vacío ahora es arrastrable (mejora, no regresión).
- Funciona en la ventana principal y en una ventana flotante (brief 09).
- `TitleBarDragExclude`/`TitleBarDragRegion` permiten override manual cuando se dibuja contenido
  interactivo custom que no pasa por `SetLastItem`.
- GL y Vulkan; tests existentes pasan.

## Fuera de alcance
- Integración con el área de caption del SO en Windows (DWM) para respetar el ancho nativo de los
  botones — mejora futura. Snap layouts de Windows 11 al hover del botón maximizar (requiere DWM).
