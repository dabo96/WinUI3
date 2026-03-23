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
15. [Espaciado y Cursor](#espaciado-y-cursor)
16. [DPI](#dpi)
17. [Style Overrides](#style-overrides)
18. [Accesibilidad](#accesibilidad)
19. [Temas y Estilos](#temas-y-estilos)
20. [Callbacks y Eventos](#callbacks-y-eventos)

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
