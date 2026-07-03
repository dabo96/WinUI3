# FluentGUI - Referencia de API

## Índice

1. [Inicialización](#inicialización)
2. [Layout](#layout)
3. [Widgets Básicos](#widgets-básicos)
4. [Drag Widgets](#drag-widgets)
5. [Contenedores](#contenedores)
6. [Splitter](#splitter)
7. [Overlays](#overlays)
8. [Listas](#listas)
9. [TreeView](#treeview)
10. [Menú](#menú)
11. [Toolbar y StatusBar](#toolbar-y-statusbar)
12. [Grid](#grid)
13. [Table / DataGrid](#table--datagrid)
14. [Widgets Especializados](#widgets-especializados)
15. [Fecha y Hora](#fecha-y-hora)
16. [Controles de Firma](#controles-de-firma)
17. [Navegación y App Shell](#navegación-y-app-shell)
18. [Feedback y Estado](#feedback-y-estado)
19. [Texto y Contenido Rico](#texto-y-contenido-rico)
20. [Espaciado y Cursor](#espaciado-y-cursor)
21. [DPI](#dpi)
22. [Style Overrides](#style-overrides)
23. [Accesibilidad](#accesibilidad)
24. [Temas y Estilos](#temas-y-estilos)
25. [Callbacks y Eventos](#callbacks-y-eventos)
26. [Iconos en Widgets](#iconos-en-widgets)

## Inicialización

### CreateContext
Crea el contexto UI principal.

```cpp
UIContext* CreateContext(SDL_Window* window);
```

**Parámetros:**
- `window`: Puntero a la ventana SDL3

**Retorna:** Puntero al contexto UI creado

### GetContext
Obtiene el contexto UI actual.

```cpp
UIContext* GetContext();
```

### NewFrame
Inicia un nuevo frame. Debe llamarse una vez por frame antes de renderizar widgets.

```cpp
void NewFrame(float deltaTime = 0.016f);
```

**Parámetros:**
- `deltaTime`: Tiempo transcurrido desde el último frame en segundos (por defecto 60 FPS)

### Render
Renderiza todos los widgets dibujados en el frame actual.

```cpp
void Render();
```

### RenderDeferredDropdowns
Renderiza overlays diferidos (dropdowns, tooltips). Llamar antes de `Render()`.

```cpp
void RenderDeferredDropdowns();
```

### DestroyContext
Destruye el contexto UI y libera recursos.

```cpp
void DestroyContext();
```

## Layout

### BeginVertical / EndVertical
Crea un layout vertical (los widgets se apilan verticalmente).

```cpp
void BeginVertical(float spacing = -1.0f,
                   std::optional<Vec2> size = std::nullopt,
                   std::optional<Vec2> padding = std::nullopt);

void EndVertical(bool advanceParent = true);
```

**Parámetros:**
- `spacing`: Espaciado en píxeles entre hijos (-1 = usar valor por defecto del estilo)
- `size`: Tamaño explícito; `std::nullopt` = automático desde el padre
- `padding`: Padding interno; `std::nullopt` = usar valor por defecto del estilo
- `advanceParent`: Si avanza la posición del layout padre

### BeginHorizontal / EndHorizontal
Crea un layout horizontal (los widgets se apilan horizontalmente).

```cpp
void BeginHorizontal(float spacing = -1.0f,
                     std::optional<Vec2> size = std::nullopt,
                     std::optional<Vec2> padding = std::nullopt);

void EndHorizontal(bool advanceParent = true);
```

### SetNextConstraints
Establece restricciones de tamaño para el siguiente widget (Fixed / Fill / Auto).

```cpp
void SetNextConstraints(const LayoutConstraints& constraints);
```

## Widgets Básicos

### Button
Crea un botón clickeable.

```cpp
bool Button(const std::string& label,
            const Vec2& size = Vec2(0, 0),
            std::optional<Vec2> pos = std::nullopt,
            bool enabled = true);
```

**Parámetros:**
- `label`: Texto del botón
- `size`: Tamaño del botón (`Vec2(0,0)` para auto-sizing)
- `pos`: Posición absoluta (`std::nullopt` para usar layout)
- `enabled`: Si el botón está habilitado

**Retorna:** `true` si se hizo click en el botón

### Label
Muestra texto estático.

```cpp
void Label(const std::string& text,
           std::optional<Vec2> position = std::nullopt,
           TypographyStyle variant = TypographyStyle::Body,
           bool disabled = false);
```

**Parámetros:**
- `text`: Texto a mostrar
- `position`: Posición opcional (`std::nullopt` para usar layout)
- `variant`: Estilo tipográfico (Caption, Body, Subtitle, Title, etc.)
- `disabled`: Si el texto está deshabilitado

### Separator
Dibuja una línea horizontal separadora.

```cpp
void Separator();
```

### Checkbox
Crea una casilla de verificación.

```cpp
bool Checkbox(const std::string& label,
              bool* value = nullptr,
              std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `label`: Etiqueta del checkbox
- `value`: Puntero al valor booleano (`nullptr` para solo visualización)
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el valor cambió

### RadioButton
Crea un botón de opción única.

```cpp
bool RadioButton(const std::string& label,
                 int* value,
                 int optionValue,
                 const std::string& group = "",
                 std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `label`: Etiqueta del radio button
- `value`: Puntero al valor actual del grupo
- `optionValue`: Valor que representa este radio button
- `group`: ID del grupo (radio buttons con el mismo group son mutuamente exclusivos)
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si se seleccionó este radio button

### SliderFloat / SliderInt
Crea un deslizador para valores numéricos.

```cpp
bool SliderFloat(const std::string& label,
                 float* value,
                 float minValue,
                 float maxValue,
                 float width = 200.0f,
                 const char* format = "%.2f",
                 std::optional<Vec2> pos = std::nullopt);

bool SliderInt(const std::string& label,
               int* value,
               int minValue,
               int maxValue,
               float width = 200.0f,
               std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `label`: Etiqueta del slider
- `value`: Puntero al valor
- `minValue` / `maxValue`: Rango de valores
- `width`: Ancho del slider en píxeles
- `format`: Formato de texto (solo SliderFloat)
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el valor cambió

### ProgressBar
Muestra una barra de progreso.

```cpp
void ProgressBar(float fraction,
                 const Vec2& size = Vec2(0, 0),
                 const std::string& overlay = "",
                 std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `fraction`: Valor entre 0.0 y 1.0
- `size`: Tamaño de la barra
- `overlay`: Texto opcional a mostrar sobre la barra
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

### TextInput
Crea un campo de texto editable. Soporta Ctrl+Z/Y (undo/redo), IME y selección con click-drag.

```cpp
bool TextInput(const std::string& label,
               std::string* value,
               float width = 200.0f,
               bool multiline = false,
               std::optional<Vec2> pos = std::nullopt,
               const char* placeholder = nullptr,
               size_t maxLength = 0);
```

**Parámetros:**
- `label`: Etiqueta del campo
- `value`: Puntero al string con el texto
- `width`: Ancho del campo en píxeles
- `multiline`: Si permite múltiples líneas
- `pos`: Posición absoluta (`std::nullopt` para usar layout)
- `placeholder`: Texto gris mostrado cuando está vacío y sin foco
- `maxLength`: Número máximo de caracteres (0 = ilimitado)

**Retorna:** `true` si el texto cambió

### ComboBox
Dropdown con navegación por teclado (Up/Down/Enter/Escape).

```cpp
bool ComboBox(const std::string& label,
              int* currentItem,
              const std::vector<std::string>& items,
              float width = 200.0f,
              std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `label`: Etiqueta del combo box
- `currentItem`: Puntero al índice del item seleccionado
- `items`: Lista de opciones
- `width`: Ancho del combo box en píxeles
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el índice seleccionado cambió

## Drag Widgets

Editores numéricos de tipo click-drag. Se arrastra horizontalmente para cambiar el valor. Doble-click para entrada por teclado.

### DragFloat
```cpp
bool DragFloat(const std::string& label,
               float* value,
               float speed = 1.0f,
               float min = 0.0f,
               float max = 0.0f,
               const char* format = "%.2f",
               std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `label`: Etiqueta del widget
- `value`: Puntero al valor float
- `speed`: Velocidad de cambio al arrastrar
- `min` / `max`: Rango de valores (0/0 = sin límites)
- `format`: Formato de texto
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el valor cambió

### DragInt
```cpp
bool DragInt(const std::string& label,
             int* value,
             float speed = 1.0f,
             int min = 0,
             int max = 0,
             std::optional<Vec2> pos = std::nullopt);
```

**Retorna:** `true` si el valor cambió

### DragFloat3
Editor de tres componentes float (por ejemplo, vectores XYZ).

```cpp
bool DragFloat3(const std::string& label,
                float values[3],
                float speed = 1.0f,
                float min = 0.0f,
                float max = 0.0f,
                const char* format = "%.2f",
                std::optional<Vec2> pos = std::nullopt);
```

**Retorna:** `true` si algún valor cambió

## Contenedores

### Panel
Crea un panel arrastrable con título y opción de minimizar.

```cpp
bool BeginPanel(const std::string& id,
                const Vec2& size = Vec2(0, 0),
                bool reserveLayoutSpace = true,
                std::optional<bool> useAcrylic = std::nullopt,
                std::optional<float> acrylicOpacity = std::nullopt,
                std::optional<Vec2> pos = std::nullopt,
                float maxHeight = 0.0f);

void EndPanel();
```

**Parámetros:**
- `id`: Identificador único del panel
- `size`: Tamaño del panel
- `reserveLayoutSpace`: Si reserva espacio en el layout padre
- `useAcrylic`: Habilitar efecto acrílico (`std::nullopt` = usar valor del estilo)
- `acrylicOpacity`: Opacidad del efecto acrílico
- `pos`: Posición absoluta (`std::nullopt` para usar layout)
- `maxHeight`: Altura máxima (0 = sin límite)

**Retorna:** `true` si el panel está visible (no minimizado)

**Características:**
- Título clickeable para minimizar/expandir
- Efecto acrílico opcional
- Arrastrable
- Soporte para widgets dentro del panel

### ScrollView
Crea una vista con scrollbars.

```cpp
bool BeginScrollView(const std::string& id,
                     const Vec2& size,
                     Vec2* scrollOffset = nullptr,
                     std::optional<Vec2> pos = std::nullopt);

void EndScrollView();
```

**Parámetros:**
- `id`: Identificador único
- `size`: Tamaño de la vista (Vec2(0,0) para llenar espacio disponible)
- `scrollOffset`: Puntero opcional al offset de scroll
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Características:**
- Scrollbars automáticos cuando el contenido excede el tamaño
- Soporte para scroll con mouse wheel
- Clipping automático del contenido

### TabView
Crea una vista con pestañas.

```cpp
bool BeginTabView(const std::string& id,
                  int* activeTab,
                  const std::vector<std::string>& tabLabels,
                  const Vec2& size = Vec2(0, 0),
                  std::optional<Vec2> pos = std::nullopt);

void EndTabView();
```

**Parámetros:**
- `id`: Identificador único
- `activeTab`: Puntero al índice de la pestaña activa
- `tabLabels`: Nombres de las pestañas
- `size`: Tamaño del contenedor
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

## Splitter

Panel divisor redimensionable de dos paneles.

```cpp
bool BeginSplitter(const std::string& id,
                   bool vertical,
                   float* ratio,
                   const Vec2& size = Vec2(0, 0));

void SplitterPanel();  // Llamar entre el contenido del primer y segundo panel
void EndSplitter();
```

**Parámetros:**
- `id`: Identificador único
- `vertical`: `true` = divisor vertical (izquierda|derecha), `false` = horizontal (arriba/abajo)
- `ratio`: Proporción de división (0.0 – 1.0), se actualiza al arrastrar
- `size`: Tamaño total del splitter

**Ejemplo:**
```cpp
static float ratio = 0.5f;
if (BeginSplitter("split1", true, &ratio, Vec2(600, 400))) {
    // Contenido del panel izquierdo
    Label("Panel Izquierdo");

    SplitterPanel();

    // Contenido del panel derecho
    Label("Panel Derecho");
}
EndSplitter();
```

## Overlays

### Tooltip
Muestra un tooltip al pasar el mouse sobre el widget anterior. Soporta multi-línea (`\n`).

```cpp
void Tooltip(const std::string& text, float delay = 0.5f);
```

**Parámetros:**
- `text`: Texto del tooltip (soporta `\n` para múltiples líneas)
- `delay`: Tiempo en segundos antes de mostrar el tooltip

### ContextMenu
Crea un menú contextual (clic derecho).

```cpp
bool BeginContextMenu(const std::string& id);
bool ContextMenuItem(const std::string& label, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();
```

**Retorna:** `BeginContextMenu` retorna `true` si el menú está abierto; `ContextMenuItem` retorna `true` cuando se hace click

### Modal
Crea una ventana modal/diálogo.

```cpp
bool BeginModal(const std::string& id,
                const std::string& title,
                bool* open,
                const Vec2& size = Vec2(400, 300));

void EndModal();
```

**Parámetros:**
- `id`: Identificador único
- `title`: Título de la ventana
- `open`: Puntero al estado abierto/cerrado
- `size`: Tamaño de la ventana

**Características:**
- Backdrop oscuro
- Arrastrable por el título
- Botón de cerrar (X)
- Se cierra con Escape

## Listas

### ListView
Crea una lista de items seleccionables. Soporta selección simple y múltiple.

```cpp
// Selección simple
bool BeginListView(const std::string& id,
                   const Vec2& size,
                   int* selectedItem,
                   const std::vector<std::string>& items,
                   std::optional<Vec2> pos = std::nullopt);

// Selección múltiple (Ctrl+Click, Shift+Click)
bool BeginListView(const std::string& id,
                   const Vec2& size,
                   std::vector<int>* selectedItems,
                   const std::vector<std::string>& items,
                   std::optional<Vec2> pos = std::nullopt);

void EndListView();
```

**Parámetros:**
- `id`: Identificador único
- `size`: Tamaño de la lista
- `selectedItem`: Puntero al índice seleccionado (selección simple)
- `selectedItems`: Puntero al vector de índices seleccionados (selección múltiple)
- `items`: Lista de textos a mostrar
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

## TreeView

### TreeNodeData
Struct auxiliar para construcción declarativa de árboles.

```cpp
struct TreeNodeData {
    std::string label;
    bool isOpen = false;
    bool isSelected = false;
    std::vector<TreeNodeData> children;

    TreeNodeData(const std::string& lbl);
};
```

### TreeView API

```cpp
bool BeginTreeView(const std::string& id,
                   const Vec2& size,
                   std::optional<Vec2> pos = std::nullopt);

bool TreeNode(const std::string& id,
              const std::string& label,
              bool* isOpen = nullptr,
              bool* isSelected = nullptr);

void TreeNodePush();  // Antes de mostrar hijos (aumentar profundidad)
void TreeNodePop();   // Después de mostrar hijos (reducir profundidad)

void EndTreeView();
```

**Parámetros de TreeNode:**
- `id`: Identificador único del nodo
- `label`: Texto del nodo
- `isOpen`: Puntero al estado expandido (pasar no-null para nodos expandibles)
- `isSelected`: Puntero al estado de selección

**Retorna:** `true` si el nodo está expandido y tiene hijos

**Ejemplo:**
```cpp
if (BeginTreeView("tree", Vec2(250, 300))) {
    static bool folder1Open = false;
    if (TreeNode("folder1", "Folder 1", &folder1Open)) {
        TreeNodePush();
        TreeNode("file1", "File 1.txt");
        TreeNode("file2", "File 2.txt");
        TreeNodePop();
    }
}
EndTreeView();
```

## Menú

### MenuBar
Crea una barra de menú horizontal.

```cpp
bool BeginMenuBar();
bool BeginMenu(const std::string& label, bool enabled = true);
bool MenuItem(const std::string& label, bool enabled = true);
void MenuSeparator();
void EndMenu();
void EndMenuBar();
```

**Ejemplo:**
```cpp
if (BeginMenuBar()) {
    if (BeginMenu("File")) {
        if (MenuItem("New")) { /* ... */ }
        if (MenuItem("Open")) { /* ... */ }
        MenuSeparator();
        if (MenuItem("Exit")) { /* ... */ }
    }
    EndMenu();
}
EndMenuBar();
```

## Toolbar y StatusBar

### Toolbar
Franja de herramientas horizontal.

```cpp
void BeginToolbar();
void EndToolbar();
```

**Ejemplo:**
```cpp
BeginToolbar();
if (Button("Save")) { /* ... */ }
if (Button("Undo")) { /* ... */ }
EndToolbar();
```

### StatusBar
Barra de estado en la parte inferior.

```cpp
void BeginStatusBar(const std::string& text = "");
void EndStatusBar();
```

**Parámetros:**
- `text`: Texto inicial a mostrar en la barra de estado

## Grid

Layout de cuadrícula con número fijo de columnas.

```cpp
void BeginGrid(const std::string& id,
               int columns,
               float rowHeight = 0.0f);

void GridNextCell();
void EndGrid();
```

**Parámetros:**
- `id`: Identificador único
- `columns`: Número de columnas
- `rowHeight`: Altura de cada fila (0 = automático)

**Ejemplo:**
```cpp
BeginGrid("myGrid", 3, 50.0f);
Label("Celda 1"); GridNextCell();
Label("Celda 2"); GridNextCell();
Label("Celda 3"); GridNextCell();
Label("Celda 4"); GridNextCell();
Label("Celda 5"); GridNextCell();
Label("Celda 6");
EndGrid();
```

## Table / DataGrid

Tabla de datos con columnas ordenables y redimensionables.

### TableColumn
```cpp
struct TableColumn {
    std::string header;       // Texto del encabezado
    float width = 100.0f;     // Ancho inicial/actual
    float minWidth = 40.0f;   // Ancho mínimo
    bool sortable = true;     // Si la columna es ordenable
};
```

### TableState
```cpp
struct TableState {
    int sortColumn = -1;       // -1 = sin ordenamiento
    bool sortAscending = true; // Orden ascendente
    float scrollOffset = 0.0f; // Offset de scroll
};
```

### Table API
```cpp
bool BeginTable(const std::string& id,
                std::vector<TableColumn>& columns,
                int rowCount,
                const Vec2& size = Vec2(0, 0),
                TableState* state = nullptr);

void TableNextRow();
void TableSetCell(int column);
void EndTable();
```

**Ejemplo:**
```cpp
static std::vector<TableColumn> cols = {
    {"Nombre", 150.0f}, {"Edad", 80.0f}, {"Ciudad", 120.0f}
};
static TableState state;

if (BeginTable("tabla1", cols, 3, Vec2(400, 200), &state)) {
    TableNextRow();
    TableSetCell(0); Label("Alice");
    TableSetCell(1); Label("30");
    TableSetCell(2); Label("Madrid");

    TableNextRow();
    TableSetCell(0); Label("Bob");
    TableSetCell(1); Label("25");
    TableSetCell(2); Label("Barcelona");

    TableNextRow();
    TableSetCell(0); Label("Charlie");
    TableSetCell(1); Label("35");
    TableSetCell(2); Label("Sevilla");
}
EndTable();
```

## Widgets Especializados

### ColorPicker
Selector de color HSV con sliders RGB y entrada hexadecimal.

```cpp
bool ColorPicker(const std::string& label,
                 Color* value,
                 std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `label`: Etiqueta del widget
- `value`: Puntero al color
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el color cambió

### Image
Muestra una textura GPU.

```cpp
void Image(const std::string& id,
           void* textureHandle,
           const Vec2& size,
           const Vec2& uv0 = Vec2(0, 0),
           const Vec2& uv1 = Vec2(1, 1),
           std::optional<Vec2> pos = std::nullopt);
```

**Parámetros:**
- `id`: Identificador único
- `textureHandle`: Handle de textura del backend (ej. `GLuint` casteado a `void*`)
- `size`: Tamaño de la imagen en píxeles
- `uv0` / `uv1`: Coordenadas UV para sub-regiones de la textura
- `pos`: Posición absoluta (`std::nullopt` para usar layout)

## Fecha y Hora

Selectores de fecha/hora con popup de calendario y ruedas. Operan sobre un
`DateTimeValue` (año/mes/día/hora/minuto) provisto por el usuario.

### DatePicker / TimePicker / DateTimePicker

```cpp
bool DatePicker(const std::string& label, DateTimeValue* value, std::optional<Vec2> pos = {});
bool TimePicker(const std::string& label, DateTimeValue* value, std::optional<Vec2> pos = {});
bool DateTimePicker(const std::string& label, DateTimeValue* value, std::optional<Vec2> pos = {});
```

Devuelven `true` el frame en que el valor cambia.

```cpp
static DateTimeValue fecha;   // persiste entre frames
if (DatePicker("Fecha de entrega", &fecha)) {
    // fecha.year / fecha.month / fecha.day actualizados
}

static DateTimeValue hora;
TimePicker("Hora", &hora);
```

---

## Controles de Firma

Controles característicos de Fluent construidos sobre el sistema de widgets. Todos
son navegables por teclado y exponen rol de accesibilidad.

### ToggleSwitch

Interruptor on/off tipo píldora (distinto del Checkbox). El thumb se anima al
conmutar.

```cpp
bool ToggleSwitch(const std::string& label, bool* value,
                  const std::string& onText = "", const std::string& offText = "",
                  std::optional<Vec2> pos = {});
```

```cpp
static bool wifi = true;
if (ToggleSwitch("Wi-Fi", &wifi, "Activado", "Desactivado")) {
    // wifi cambió
}
```

### Expander

Card colapsable con chevron. El contenido entre `BeginExpander`/`EndExpander` solo
se construye si está expandido; la altura se anima al abrir/cerrar.

```cpp
bool BeginExpander(const std::string& id, const std::string& header,
                   uint32_t icon = 0, bool* expanded = nullptr);
void EndExpander();
```

```cpp
if (BeginExpander("adv", "Opciones avanzadas", Icons::Settings)) {
    Checkbox("Modo experto", &expert);
    SliderInt("Nivel", &level, 0, 10);
    EndExpander();
}
```

### SplitButton / DropDownButton

`SplitButton` = acción primaria + zona desplegable (dos hit-rects). Devuelve `1`
si se invocó la acción primaria, `2` si se abrió el menú. `DropDownButton` solo
abre un menú al pulsar.

```cpp
int  SplitButton(const std::string& label, uint32_t icon,
                 std::function<void()> onPrimary,
                 const std::vector<CommandItem>& menu);
void DropDownButton(const std::string& label, uint32_t icon,
                    const std::vector<CommandItem>& menu);
```

```cpp
SplitButton("Guardar", Icons::Save,
    []{ guardar(); },
    { {"Guardar como…", Icons::FilePlus, []{ guardarComo(); }},
      {"Guardar todo",  Icons::Files,    []{ guardarTodo(); }} });
```

`CommandItem`: `{ std::string label; uint32_t icon; std::function<void()> onInvoke;
bool isPrimary; bool enabled; }`.

### NumberBox

Campo numérico con spinners +/−, validación y clamp a `[min, max]`. Responde a la
rueda del ratón (±`step`).

```cpp
bool NumberBox(const std::string& label, double* value,
               double min = -1e308, double max = 1e308, double step = 1.0,
               const char* format = "%.0f", std::optional<Vec2> pos = {});
```

```cpp
static double cantidad = 1;
NumberBox("Cantidad", &cantidad, 0, 999, 1, "%.0f");

static double precio = 9.99;
NumberBox("Precio", &precio, 0.0, 1e6, 0.01, "%.2f");
```

### Flyout / MenuFlyout

`Flyout` es una primitiva de popup anclado con contenido arbitrario (base de
SplitButton, AutoSuggestBox, etc.). Se abre/cierra por `id`, hace *flip* al borde y
se cierra al clicar fuera o con Esc. `MenuFlyout` es un menú construido encima con
iconos, acelerador y ítems checkable.

```cpp
bool BeginFlyout(const std::string& id, const Rect& anchorRect,
                 FlyoutPlacement placement = FlyoutPlacement::Bottom);
void EndFlyout();
void OpenFlyout(const std::string& id);
void CloseFlyout(const std::string& id);
bool IsFlyoutOpen(const std::string& id);

void MenuFlyout(const std::string& id, const Rect& anchorRect,
                const std::vector<MenuEntry>& entries, float staggerMs = 18.0f);
```

`FlyoutPlacement`: `Bottom, Top, Left, Right, BottomEdgeAlignedLeft,
BottomEdgeAlignedRight, TopEdgeAlignedLeft`.
`MenuEntry`: `{ label; icon; accelerator; checkable; checked; separator; enabled;
submenu; onInvoke; }`.

```cpp
// Flyout con contenido propio
if (Button("Filtros")) OpenFlyout("filtros");
Rect anchor = { lastItemPos, lastItemSize };
if (BeginFlyout("filtros", anchor)) {
    Checkbox("Solo activos", &soloActivos);
    Checkbox("Con stock", &conStock);
    EndFlyout();
}

// Menú contextual rico
MenuFlyout("acciones", anchor, {
    {"Copiar",  Icons::Copy,  "Ctrl+C"},
    {"Pegar",   Icons::Clipboard, "Ctrl+V"},
    {.separator = true},
    {"Eliminar", Icons::Trash, "Supr", false, false, false, true, {}, []{ borrar(); }},
});
```

### TeachingTip

Popover con "beak" (flecha) que apunta a un elemento, para onboarding. Se muestra
hasta que el usuario lo cierra (el estado "visto" se persiste por `id`).

```cpp
bool TeachingTip(const std::string& id, const Rect& targetRect,
                 const std::string& title, const std::string& body,
                 const std::string& actionText = "");
```

```cpp
Rect target = { botonPos, botonSize };
TeachingTip("tip-export", target,
            "Nuevo: Exportar a PDF",
            "Ahora puedes exportar tu informe con un clic.",
            "Entendido");
```

### RatingControl

N estrellas con preview al hover; click fija el valor. Soporta medios valores.

```cpp
bool RatingControl(const std::string& id, int* value, int maxStars = 5,
                   bool allowHalf = false);
```

```cpp
static int valoracion = 4;
if (RatingControl("rating", &valoracion, 5)) { /* cambió */ }
```

### ContentDialog

Diálogo estandarizado sobre el Modal, con scrim, botones estándar y **resultado**.
Atrapa el foco; Esc = Close, Enter = Primary. Al decidirse, pone `*open = false`.

```cpp
enum class DialogResult { None, Primary, Secondary, Close };
DialogResult ContentDialog(const std::string& id, bool* open,
                           const std::string& title,
                           std::function<void()> body,
                           const std::string& primaryText  = "OK",
                           const std::string& secondaryText = "",
                           const std::string& closeText     = "Cancel");
```

```cpp
static bool confirmar = false;
if (Button("Eliminar")) confirmar = true;

auto r = ContentDialog("del", &confirmar, "¿Eliminar registro?",
    []{ Label("Esta acción no se puede deshacer."); },
    "Eliminar", "", "Cancelar");
if (r == DialogResult::Primary) eliminarRegistro();
```

---

## Navegación y App Shell

Piezas para estructurar una aplicación completa: panel de navegación lateral,
navegación por páginas con historial, barra de comandos con overflow, breadcrumbs y
barra de título custom.

### NavigationView

Panel lateral Fluent: filas con icono + label, selección con barra de acento,
sub-ítems con chevron, hamburguesa que alterna el modo y `footerItems` anclados
abajo. Es responsive (se colapsa a Compact/Minimal al estrechar la ventana).

```cpp
struct NavItem { std::string key; std::string label; uint32_t icon = 0;
                 int badge = 0; std::vector<NavItem> children; };
enum class NavDisplayMode { Expanded, Compact, Minimal };

std::string NavigationView(const std::string& id,
                           const std::vector<NavItem>& items,
                           std::string* selectedKey,
                           NavDisplayMode mode = NavDisplayMode::Expanded,
                           const std::vector<NavItem>& footerItems = {});
```

```cpp
static std::string page = "inicio";
std::vector<NavItem> items = {
    {"inicio",    "Inicio",     Icons::House},
    {"inventario","Inventario", Icons::Box, 3 /* badge */},
    {"informes",  "Informes",   Icons::ChartBar},
};
std::vector<NavItem> footer = { {"ajustes", "Ajustes", Icons::Settings} };

BeginHorizontal();
    page = NavigationView("nav", items, &page, NavDisplayMode::Expanded, footer);
    BeginVertical();                 // content host
        if (page == "inicio")     PaginaInicio();
        else if (page == "inventario") PaginaInventario();
        // …
    EndVertical();
EndHorizontal();
```

### NavFrame (navegación por páginas)

Historial back/forward ligero (POD retenido por el usuario).

```cpp
struct NavFrame { std::vector<std::string> backStack, forwardStack; std::string current; };
void NavigateTo(NavFrame& f, const std::string& pageKey);
bool NavigateBack(NavFrame& f);
bool NavigateForward(NavFrame& f);
```

```cpp
static NavFrame frame; if (frame.current.empty()) frame.current = "lista";
if (IconButton(Icons::ArrowLeft)) NavigateBack(frame);
if (abrirDetalle) NavigateTo(frame, "detalle");
if (frame.current == "lista") Lista(); else Detalle();
```

### CommandBar

Barra de comandos con overflow: coloca `primary` mientras quepan; los que no caben
+ `secondary` van a un menú "···". Se recalcula en cada resize.

```cpp
void CommandBar(const std::string& id,
                const std::vector<CommandItem>& primary,
                const std::vector<CommandItem>& secondary = {});
```

```cpp
CommandBar("cmd",
    { {"Nuevo", Icons::Plus, []{ nuevo(); }},
      {"Editar", Icons::Pencil, []{ editar(); }},
      {"Borrar", Icons::Trash, []{ borrar(); }} },
    { {"Importar", Icons::Upload, []{ importar(); }},
      {"Exportar", Icons::Download, []{ exportar(); }} });
```

### BreadcrumbBar

Migas separadas por chevron; todas clicables salvo la última. Colapsa las
intermedias en "…" si no caben.

```cpp
int BreadcrumbBar(const std::string& id, const std::vector<std::string>& crumbs);
```

```cpp
int clic = BreadcrumbBar("bc", { "Inicio", "Inventario", "Categoría", "Producto" });
if (clic >= 0) navegarANivel(clic);
```

### TitleBar (window chrome)

Barra de título custom: icono + título, `centerContent` opcional y caption buttons
(minimizar/maximizar/cerrar). Publica las zonas draggable para el hit-test de la
ventana. Requiere `AppConfig::useCustomTitleBar = true` al crear la `FluentApp`.

```cpp
struct TitleBarResult { bool minimizePressed, maximizePressed, closePressed; };
TitleBarResult TitleBar(const std::string& id, const std::string& title,
                        uint32_t icon = 0, std::function<void()> centerContent = nullptr);
```

```cpp
// FluentApp app("Mi App", cfg);  con cfg.useCustomTitleBar = true
TitleBar("titlebar", "Sistema de Inventario", Icons::Box,
         []{ /* p.ej. una CommandBar o un buscador al centro */ });
```

---

## Feedback y Estado

Superficie de feedback de aplicación: mensajes inline, notificaciones transitorias,
progreso, contadores y placeholders de carga.

### InfoBar

Mensaje inline con severidad, título, mensaje multilínea, acción y dismiss. El
estado "cerrado" se persiste por `id`. Devuelve `false` cuando el usuario lo cierra.

```cpp
enum class InfoSeverity { Informational, Success, Warning, Error };
bool InfoBar(const std::string& id, InfoSeverity severity,
             const std::string& title, const std::string& message,
             bool closable = true, const std::string& actionText = "");
```

```cpp
if (!guardadoOk)
    InfoBar("save-err", InfoSeverity::Error,
            "No se pudo guardar", "Comprueba tu conexión e inténtalo de nuevo.",
            true, "Reintentar");
```

### Toast / Notification host

Notificaciones transitorias en una esquina. `ShowToast` encola desde cualquier
punto; `RenderToasts` se llama **una vez por frame** (al final) para dibujarlas.

```cpp
struct ToastOptions { InfoSeverity severity = InfoSeverity::Informational;
                      float durationSec = 4.0f; std::string actionText;
                      std::function<void()> onAction; };
void ShowToast(const std::string& title, const std::string& message,
               const ToastOptions& opts = {});
void RenderToasts(UIContext* ctx);
```

```cpp
if (Button("Guardar")) {
    guardar();
    ShowToast("Guardado", "Los cambios se guardaron correctamente.",
              { .severity = InfoSeverity::Success });
}
// … al final del frame:
RenderToasts(GetContext());
```

### ProgressRing

Spinner circular. `progress` en `[0,1]` = determinado (arco proporcional);
`progress < 0` = indeterminado (gira).

```cpp
void ProgressRing(const std::string& id, float size = 32.0f, float progress = -1.0f);
```

```cpp
if (cargando) ProgressRing("load", 40.0f);          // indeterminado
else          ProgressRing("prog", 40.0f, avance);  // avance en [0,1]
```

### Badge

Píldora de acento con contador ("99+" si excede) o, con `dot = true`, un punto. Por
defecto se ancla a la esquina superior-derecha del último item dibujado.

```cpp
bool Badge(int count, bool dot = false, std::optional<Vec2> anchorTopRight = {});
```

```cpp
IconButton(Icons::Bell);
Badge(notificaciones);          // contador sobre la campana
```

### Skeleton / SkeletonText

Placeholders con shimmer mientras carga el contenido.

```cpp
void Skeleton(const Vec2& size, float cornerRadius = 4.0f);
void SkeletonText(int lines, float lineHeight = 16.0f, float lastLineFraction = 0.6f);
```

```cpp
if (cargando) {
    Skeleton({200, 120});      // bloque (p.ej. una imagen)
    SkeletonText(3);           // 3 líneas de párrafo
} else {
    // contenido real
}
```

---

## Texto y Contenido Rico

Enlaces, sugerencias, etiquetas, contraseñas y render de Markdown.

### HyperlinkButton

Texto en color de acento, subrayado en hover y cursor de mano. Si se da `url`, la
abre con el SO; si no, úsalo como botón de acción.

```cpp
bool HyperlinkButton(const std::string& text, const std::string& url = "", float fontSize = 0);
```

```cpp
if (HyperlinkButton("Ver documentación", "https://docs.ejemplo.com")) { /* abierto por el SO */ }
if (HyperlinkButton("Olvidé mi contraseña")) mostrarRecuperacion();
```

### AutoSuggestBox

Campo de búsqueda con sugerencias en popup. `suggestionsFn` recibe el texto actual
y devuelve las sugerencias. Devuelve la sugerencia elegida este frame (vacío si
ninguna).

```cpp
std::string AutoSuggestBox(const std::string& id, std::string* text,
    const std::function<std::vector<std::string>(const std::string&)>& suggestionsFn,
    const std::string& placeholder = "");
```

```cpp
static std::string query;
std::string elegido = AutoSuggestBox("buscar", &query,
    [](const std::string& q){ return buscarProductos(q); },
    "Buscar producto…");
if (!elegido.empty()) seleccionarProducto(elegido);
```

### TokenizingTextBox (chips)

Entrada de múltiples valores como "chips". Enter/coma confirma; Backspace en campo
vacío borra el último. Sugerencias opcionales.

```cpp
bool TokenizingTextBox(const std::string& id, std::vector<std::string>* tokens,
    const std::string& placeholder = "",
    const std::function<std::vector<std::string>(const std::string&)>& suggestionsFn = {});
```

```cpp
static std::vector<std::string> etiquetas;
TokenizingTextBox("tags", &etiquetas, "Añadir etiqueta…");
```

### PasswordBox

Campo de texto enmascarado con botón de revelar.

```cpp
bool PasswordBox(const std::string& id, std::string* value,
                 const std::string& placeholder = "",
                 std::optional<Vec2> pos = {}, float width = 300.0f);
```

```cpp
static std::string pass;
PasswordBox("pwd", &pass, "Contraseña");
```

### MarkdownView

Render de un subconjunto de Markdown (encabezados, énfasis inline, listas, código,
enlaces, imágenes). Solo lectura. Registra imágenes con `MarkdownRegisterImage`.

```cpp
void MarkdownView(const std::string& id, const std::string& markdown, float maxWidth = 0);
void MarkdownRegisterImage(const std::string& url, void* textureHandle, Vec2 size);
```

```cpp
MarkdownView("ayuda", R"(
# Ayuda
Usa **Ctrl+S** para guardar. Consulta la [documentación](https://docs.ejemplo.com).

- Primer paso
- Segundo paso
)");
```

## Espaciado y Cursor

### Spacing
Agrega espaciado vertical.

```cpp
void Spacing(float pixels);
```

### SameLine
Continúa renderizando en la misma línea horizontal.

```cpp
void SameLine(float offset = 0.0f);
```

**Parámetros:**
- `offset`: Offset horizontal adicional en píxeles

## DPI

### GetDPIScale
Obtiene el factor de escala actual del display (1.0 = 100%).

```cpp
float GetDPIScale();
```

### Scaled
Escala un valor en píxeles por el factor DPI actual.

```cpp
float Scaled(float value);
```

## Style Overrides

Permiten aplicar estilos temporales a widgets usando un sistema de pila (push/pop).

```cpp
void PushStyle(const Style& override);
void PopStyle();

void PushButtonStyle(const ButtonStyle& s);
void PopButtonStyle();

void PushPanelStyle(const PanelStyle& s);
void PopPanelStyle();

void PushTextColor(const Color& color);
void PopTextColor();
```

**Ejemplo:**
```cpp
PushTextColor(Color(1.0f, 0.0f, 0.0f, 1.0f)); // Texto rojo
Label("Este texto es rojo");
PopTextColor();

PushButtonStyle(customButtonStyle);
Button("Botón personalizado");
PopButtonStyle();
```

## Accesibilidad

### DrawAccessibilityFocusRing
Dibuja un anillo de foco de 2px alrededor de un widget para navegación por teclado.

```cpp
void DrawAccessibilityFocusRing(const Vec2& pos, const Vec2& size);
```

**Parámetros:**
- `pos`: Posición del widget
- `size`: Tamaño del widget

## Temas y Estilos

### Cambiar Tema
```cpp
// Tema oscuro (por defecto)
ctx->style = FluentUI::GetDarkFluentStyle();

// Tema claro
ctx->style = FluentUI::GetDefaultFluentStyle();

// Tema personalizado
ctx->style = FluentUI::CreateCustomFluentStyle(
    FluentUI::FluentColors::AccentPurple, // Color de acento
    true // darkTheme
);
```

### Colores Disponibles
```cpp
namespace FluentColors {
    // Acentos predefinidos
    AccentBlue, AccentGreen, AccentPurple,
    AccentOrange, AccentPink, AccentTeal

    // Colores base
    Background, Surface, SurfaceAlt, SurfaceElevated
    BackgroundDark, SurfaceDark, SurfaceAltDark, SurfaceElevatedDark

    // Texto
    TextPrimary, TextSecondary, TextTertiary
    TextPrimaryDark, TextSecondaryDark, TextTertiaryDark

    // Estados
    Error, Success, Warning, Info, Disabled
}
```

### Estilos Tipográficos
```cpp
enum class TypographyStyle {
    Caption,        // 12px
    Body,           // 14px
    BodyStrong,     // 14px Bold
    Subtitle,       // 18px
    SubtitleStrong, // 18px Bold
    Title,          // 20px
    TitleLarge,     // 28px
    Display         // 42px
};
```

## Callbacks y Eventos

### Registrar Callback
```cpp
ctx->RegisterCallback(widgetId, []() {
    // Acción cuando el widget se activa
});
```

### Registrar Callback de Cambio de Valor
```cpp
ctx->RegisterValueChanged("mySlider",
    [](const std::string& id, void* value) {
        float* fval = static_cast<float*>(value);
        // Acción cuando el valor cambia
    });
```

## Iconos en Widgets

FluentUI viene con la fuente [Lucide](https://lucide.dev) preempaquetada y
**la carga automáticamente al construir `FluentApp`**. No necesitas tocar
`LoadIconFont` ni el `.ttf`: simplemente pasa cualquier valor de
`Icons::*` a un widget icon-aware.

```cpp
#include <FluentUI/API.h>
using namespace FluentUI;

if (Button("Guardar", Icons::Save)) save();

SegmentedControl("tool", {
    {"", Icons::Pointer}, {"", Icons::Move},
    {"", Icons::Rotate},  {"", Icons::Scale},
}, &tool);
```

`Icons::Save`, `Icons::Pointer`, etc. son entradas de un `enum Lucide : uint32_t`
generado a partir del `.ttf` real, así que se convierten implícitamente al
codepoint que esperan los overloads de los widgets. El catálogo completo
(~1500 iconos) vive en `include/UI/Icons.h`.

### 1. Cómo encuentra la fuente

Al construir un `FluentApp`, la librería busca `lucide.ttf` en este orden:

1. `AppConfig::iconFontPath` si lo proporcionas explícitamente.
2. `<directorio del ejecutable>/assets/fonts/lucide.ttf` (ruta canónica vendorizada).
3. `<cwd>/assets/fonts/lucide.ttf`.
4. `$FLUENTUI_ASSETS_DIR/fonts/lucide.ttf`.

Si nada existe, la app sigue corriendo (los iconos quedan invisibles, no
crashea) y se loguea un warning. CMake copia la fuente al directorio del
binario automáticamente, así que en builds estándar nunca tienes que
preocuparte.

### Override avanzado (sustituir la fuente)

Si quieres usar otra fuente (Font Awesome, Fluent UI System Icons, tu set
custom de IcoMoon, etc.):

```cpp
AppConfig cfg;
cfg.iconFontPath = "C:/assets/fa-solid.ttf";
cfg.iconFontSize = 18;
FluentApp app("MyApp", cfg);

\ Tus codepoints van directos como uint32_t (sin Icons::):
if (Button("Save", 0xF0C7 /* fa-save */)) save();
```

El catálogo `Icons::*` solo es válido para el `.ttf` de Lucide vendorizado —
si lo reemplazas, define tus propios constantes o regenera el header con
`tools/gen_icons.py` apuntando a tu nueva fuente.

### 2. API expuesta

Cada widget icon-aware tiene un overload con `uint32_t iconCodepoint` (0 = sin
icono). El icono no afecta los IDs ni la lógica del widget — solo se dibuja.

| Widget | Posición del icono | Notas |
|---|---|---|
| `Button(label, iconCp, …)` | Izquierda del texto | Pasa `label=""` → botón cuadrado solo-icono |
| `Button(label, iconCp, ButtonSize, …)` | Izquierda | Variante S/M/L |
| `IconButton(iconCp, size?, …)` | Centrado | Atajo idiomático para toolbars |
| `Label(text, iconCp, …)` | Izquierda del texto | |
| `IconLabel(iconCp, size?, color?, …)` | — | Icono suelto, sin texto |
| `Checkbox(label, iconCp, …)` | Después del label | Útil para badges |
| `RadioButton(label, iconCp, …)` | Después del label | |
| `MenuItem(label, iconCp, …)` | Izquierda | Paridad con `BeginMenu` |
| `BeginMenu(label, iconCp, …)` | Izquierda | Ya existía en versiones previas |
| `ContextMenuItem(label, iconCp, …)` | Izquierda | |
| `BeginPanel(id, iconCp, …)` | En la barra de título | |
| `BeginModal(id, title, iconCp, …)` | En el header | Ideal para warning/info/error |
| `BeginTabView(id, ptr, vector<pair<str,cp>>, …)` | Por tab | Pasa `(label, codepoint)` por tab |
| `ComboBox(label, ptr, vector<pair<str,cp>>, …)` | Izquierda del item (dropdown + campo) | También aplica a `ComboBoxSearchable` |
| `SegmentedControl(id, vector<pair<str,cp>>, …)` | Por segmento | `label=""` → segmento cuadrado |
| `BeginListView(…, vector<pair<str,cp>>, …)` | Por fila | Single y multi-selección |
| `TableColumn{header, …, iconCodepoint}` | En el header de la columna | Campo en la struct |
| `TreeNode(id, label, iconCp, …)` | Después del chevron | |
| `TreeNodeMulti(id, label, iconCp, …)` | Después del chevron | |

Widgets sin overload (por diseño): `SliderFloat/Int`, `DragFloat/Int/3`,
`ProgressBar`, `ColorPicker`, `TextInput`, `DatePicker`/`TimePicker`,
`Plot*`, `Image`, `Separator`, layout primitives.

### 3. Ejemplos

**ComboBox de assets (campo + dropdown con icono por item):**
```cpp
int meshIdx = 0;
ComboBox("Mesh", &meshIdx, {
    {"PlayerBody.fbx", Icons::Get("box")},
    {"Enemy.fbx",      Icons::Get("box")},
    {"Crate.fbx",      Icons::Get("package")},
}, 200.0f);
```
El icono se dibuja a la izquierda del texto seleccionado (en el campo cerrado)
y a la izquierda de cada fila del dropdown. Cuando un item tiene icono se
sustituye el "selection dot" por el icono coloreado con el accent.

**Toolbar de transformaciones (tu primera captura):**
```cpp
BeginToolbar();
    int tool = 0;
    SegmentedControl("tool", {
        {"", 0xE92Au /*mouse-pointer*/},
        {"", 0xE93Cu /*move*/},
        {"", 0xE948u /*rotate*/},
        {"", 0xE954u /*scale*/}
    }, &tool);
EndToolbar();
```

**Hierarchy con carpetas (tu tercera captura):**
```cpp
if (BeginTreeView("hier", Vec2(220, 0))) {
    bool aOpen = true;
    if (TreeNode("assets", "Assets", 0xE823u /*folder*/, &aOpen)) {
        TreeNodePush();
        TreeNode("audio", "Audio", 0xE823u);
        TreeNode("anim",  "Animations", 0xE823u);
        TreeNodePop();
    }
    EndTreeView();
}
```

**Status bar inline (tu segunda captura):**
```cpp
BeginStatusBar();
    BeginHorizontal();
        IconLabel(0xE901u /*activity*/, 14.0f, Color(0.4f, 1.0f, 0.4f, 1.0f));
        Label("OpenGL 4.6 - RTX 4070");
    EndHorizontal();
EndStatusBar();
```

### Catálogo `Icons::*`

El header `include/UI/Icons.h` se genera automáticamente desde el `.ttf`
vendorizado y contiene un `enum Lucide : uint32_t` con TODOS los iconos
(actualmente ~1500). Como es un `enum` no-scoped, **se convierte implícitamente
a `uint32_t`** y entra en cualquier overload sin `static_cast`:

```cpp
Button("Save", Icons::Save);
TreeNode("a", "Assets", Icons::Folder, &open);
TableColumn{"Name", 160, 40, true, Icons::FileText};
```

**Nombres canónicos** — equivalentes a los que ves en
[lucide.dev](https://lucide.dev), convertidos de kebab-case a PascalCase:
`mouse-pointer` → `MousePointer`, `arrow-up-right` → `ArrowUpRight`,
`rotate-cw` → `RotateCw`.

**Aliases semánticos** — algunos nombres más cortos para el día a día en un
editor:

| Alias | Equivale a | Uso típico |
|---|---|---|
| `Icons::Pointer` | `MousePointer` | Selección |
| `Icons::Stop` | `Square` | Detener reproducción |
| `Icons::Cut` | `Scissors` | Edit menu |
| `Icons::Paste` | `Clipboard` | Edit menu |
| `Icons::Open` | `FolderOpen` | File menu |
| `Icons::New` | `FilePlus` | File menu |
| `Icons::Close` | `X` | Botón de cerrar |
| `Icons::Help` | `CircleHelp` | Tooltips, F1 |
| `Icons::Cube` | `Box` | Mesh, geometría |
| `Icons::Record` | `Circle` | Captura |
| `Icons::Rotate` | `RotateCw` | Tool gizmo |
| `Icons::Unlock` | `LockOpen` | Toggle de bloqueo |

Si necesitas algún icono que no encuentras: abre `include/UI/Icons.h` y busca
en el enum (autocompletado del IDE en `Icons::` también funciona). Categorías
típicas: archivos (`Folder*`, `File*`, `Save`, `Download`), edición (`Copy`,
`Pencil`, `Trash`, `Scissors`), transform (`Move`, `RotateCw`, `Scale`,
`Box`), reproducción (`Play`, `Pause`, `Square`), estado (`Check`, `X`,
`Info`, `TriangleAlert`, `CircleHelp`), navegación (`Chevron*`, `Arrow*`,
`House`), sistema (`Settings`, `Terminal`, `Monitor`, `Cpu`, `Database`).

### Regenerar el catálogo

Para actualizar la versión de Lucide:

```bash
# 1) Reemplaza assets/fonts/lucide.ttf con la versión nueva
# 2) Regenera el header
pip install fonttools
python tools/gen_icons.py
```

El script lee el `.ttf` directamente con `fontTools` y reescribe
`include/UI/Icons.h` con los codepoints correctos. Commitea ambos archivos.

