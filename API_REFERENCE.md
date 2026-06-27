# FluentGUI - Referencia de API

## Ãndice
1. [InicializaciÃ³n](#inicializaciÃ³n)
2. [Layout](#layout)
3. [Widgets BÃ¡sicos](#widgets-bÃ¡sicos)
4. [Drag Widgets](#drag-widgets)
5. [Contenedores](#contenedores)
6. [Splitter](#splitter)
7. [Overlays](#overlays)
8. [Listas](#listas)
9. [TreeView](#treeview)
10. [MenÃº](#menÃº)
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

## InicializaciÃ³n

### CreateContext
Crea el contexto UI principal.

```cpp
UIContext* CreateContext(SDL_Window* window);
```

**ParÃ¡metros:**
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

**ParÃ¡metros:**
- `deltaTime`: Tiempo transcurrido desde el Ãºltimo frame en segundos (por defecto 60 FPS)

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

**ParÃ¡metros:**
- `spacing`: Espaciado en pÃ­xeles entre hijos (-1 = usar valor por defecto del estilo)
- `size`: TamaÃ±o explÃ­cito; `std::nullopt` = automÃ¡tico desde el padre
- `padding`: Padding interno; `std::nullopt` = usar valor por defecto del estilo
- `advanceParent`: Si avanza la posiciÃ³n del layout padre

### BeginHorizontal / EndHorizontal
Crea un layout horizontal (los widgets se apilan horizontalmente).

```cpp
void BeginHorizontal(float spacing = -1.0f,
                     std::optional<Vec2> size = std::nullopt,
                     std::optional<Vec2> padding = std::nullopt);

void EndHorizontal(bool advanceParent = true);
```

### SetNextConstraints
Establece restricciones de tamaÃ±o para el siguiente widget (Fixed / Fill / Auto).

```cpp
void SetNextConstraints(const LayoutConstraints& constraints);
```

## Widgets BÃ¡sicos

### Button
Crea un botÃ³n clickeable.

```cpp
bool Button(const std::string& label,
            const Vec2& size = Vec2(0, 0),
            std::optional<Vec2> pos = std::nullopt,
            bool enabled = true);
```

**ParÃ¡metros:**
- `label`: Texto del botÃ³n
- `size`: TamaÃ±o del botÃ³n (`Vec2(0,0)` para auto-sizing)
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)
- `enabled`: Si el botÃ³n estÃ¡ habilitado

**Retorna:** `true` si se hizo click en el botÃ³n

### Label
Muestra texto estÃ¡tico.

```cpp
void Label(const std::string& text,
           std::optional<Vec2> position = std::nullopt,
           TypographyStyle variant = TypographyStyle::Body,
           bool disabled = false);
```

**ParÃ¡metros:**
- `text`: Texto a mostrar
- `position`: PosiciÃ³n opcional (`std::nullopt` para usar layout)
- `variant`: Estilo tipogrÃ¡fico (Caption, Body, Subtitle, Title, etc.)
- `disabled`: Si el texto estÃ¡ deshabilitado

### Separator
Dibuja una lÃ­nea horizontal separadora.

```cpp
void Separator();
```

### Checkbox
Crea una casilla de verificaciÃ³n.

```cpp
bool Checkbox(const std::string& label,
              bool* value = nullptr,
              std::optional<Vec2> pos = std::nullopt);
```

**ParÃ¡metros:**
- `label`: Etiqueta del checkbox
- `value`: Puntero al valor booleano (`nullptr` para solo visualizaciÃ³n)
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el valor cambiÃ³

### RadioButton
Crea un botÃ³n de opciÃ³n Ãºnica.

```cpp
bool RadioButton(const std::string& label,
                 int* value,
                 int optionValue,
                 const std::string& group = "",
                 std::optional<Vec2> pos = std::nullopt);
```

**ParÃ¡metros:**
- `label`: Etiqueta del radio button
- `value`: Puntero al valor actual del grupo
- `optionValue`: Valor que representa este radio button
- `group`: ID del grupo (radio buttons con el mismo group son mutuamente exclusivos)
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si se seleccionÃ³ este radio button

### SliderFloat / SliderInt
Crea un deslizador para valores numÃ©ricos.

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

**ParÃ¡metros:**
- `label`: Etiqueta del slider
- `value`: Puntero al valor
- `minValue` / `maxValue`: Rango de valores
- `width`: Ancho del slider en pÃ­xeles
- `format`: Formato de texto (solo SliderFloat)
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el valor cambiÃ³

### ProgressBar
Muestra una barra de progreso.

```cpp
void ProgressBar(float fraction,
                 const Vec2& size = Vec2(0, 0),
                 const std::string& overlay = "",
                 std::optional<Vec2> pos = std::nullopt);
```

**ParÃ¡metros:**
- `fraction`: Valor entre 0.0 y 1.0
- `size`: TamaÃ±o de la barra
- `overlay`: Texto opcional a mostrar sobre la barra
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

### TextInput
Crea un campo de texto editable. Soporta Ctrl+Z/Y (undo/redo), IME y selecciÃ³n con click-drag.

```cpp
bool TextInput(const std::string& label,
               std::string* value,
               float width = 200.0f,
               bool multiline = false,
               std::optional<Vec2> pos = std::nullopt,
               const char* placeholder = nullptr,
               size_t maxLength = 0);
```

**ParÃ¡metros:**
- `label`: Etiqueta del campo
- `value`: Puntero al string con el texto
- `width`: Ancho del campo en pÃ­xeles
- `multiline`: Si permite mÃºltiples lÃ­neas
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)
- `placeholder`: Texto gris mostrado cuando estÃ¡ vacÃ­o y sin foco
- `maxLength`: NÃºmero mÃ¡ximo de caracteres (0 = ilimitado)

**Retorna:** `true` si el texto cambiÃ³

### ComboBox
Dropdown con navegaciÃ³n por teclado (Up/Down/Enter/Escape).

```cpp
bool ComboBox(const std::string& label,
              int* currentItem,
              const std::vector<std::string>& items,
              float width = 200.0f,
              std::optional<Vec2> pos = std::nullopt);
```

**ParÃ¡metros:**
- `label`: Etiqueta del combo box
- `currentItem`: Puntero al Ã­ndice del item seleccionado
- `items`: Lista de opciones
- `width`: Ancho del combo box en pÃ­xeles
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el Ã­ndice seleccionado cambiÃ³

## Drag Widgets

Editores numÃ©ricos de tipo click-drag. Se arrastra horizontalmente para cambiar el valor. Doble-click para entrada por teclado.

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

**ParÃ¡metros:**
- `label`: Etiqueta del widget
- `value`: Puntero al valor float
- `speed`: Velocidad de cambio al arrastrar
- `min` / `max`: Rango de valores (0/0 = sin lÃ­mites)
- `format`: Formato de texto
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el valor cambiÃ³

### DragInt
```cpp
bool DragInt(const std::string& label,
             int* value,
             float speed = 1.0f,
             int min = 0,
             int max = 0,
             std::optional<Vec2> pos = std::nullopt);
```

**Retorna:** `true` si el valor cambiÃ³

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

**Retorna:** `true` si algÃºn valor cambiÃ³

## Contenedores

### Panel
Crea un panel arrastrable con tÃ­tulo y opciÃ³n de minimizar.

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

**ParÃ¡metros:**
- `id`: Identificador Ãºnico del panel
- `size`: TamaÃ±o del panel
- `reserveLayoutSpace`: Si reserva espacio en el layout padre
- `useAcrylic`: Habilitar efecto acrÃ­lico (`std::nullopt` = usar valor del estilo)
- `acrylicOpacity`: Opacidad del efecto acrÃ­lico
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)
- `maxHeight`: Altura mÃ¡xima (0 = sin lÃ­mite)

**Retorna:** `true` si el panel estÃ¡ visible (no minimizado)

**CaracterÃ­sticas:**
- TÃ­tulo clickeable para minimizar/expandir
- Efecto acrÃ­lico opcional
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

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `size`: TamaÃ±o de la vista (Vec2(0,0) para llenar espacio disponible)
- `scrollOffset`: Puntero opcional al offset de scroll
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**CaracterÃ­sticas:**
- Scrollbars automÃ¡ticos cuando el contenido excede el tamaÃ±o
- Soporte para scroll con mouse wheel
- Clipping automÃ¡tico del contenido

### TabView
Crea una vista con pestaÃ±as.

```cpp
bool BeginTabView(const std::string& id,
                  int* activeTab,
                  const std::vector<std::string>& tabLabels,
                  const Vec2& size = Vec2(0, 0),
                  std::optional<Vec2> pos = std::nullopt);

void EndTabView();
```

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `activeTab`: Puntero al Ã­ndice de la pestaÃ±a activa
- `tabLabels`: Nombres de las pestaÃ±as
- `size`: TamaÃ±o del contenedor
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

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

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `vertical`: `true` = divisor vertical (izquierda|derecha), `false` = horizontal (arriba/abajo)
- `ratio`: ProporciÃ³n de divisiÃ³n (0.0 â€“ 1.0), se actualiza al arrastrar
- `size`: TamaÃ±o total del splitter

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
Muestra un tooltip al pasar el mouse sobre el widget anterior. Soporta multi-lÃ­nea (`\n`).

```cpp
void Tooltip(const std::string& text, float delay = 0.5f);
```

**ParÃ¡metros:**
- `text`: Texto del tooltip (soporta `\n` para mÃºltiples lÃ­neas)
- `delay`: Tiempo en segundos antes de mostrar el tooltip

### ContextMenu
Crea un menÃº contextual (clic derecho).

```cpp
bool BeginContextMenu(const std::string& id);
bool ContextMenuItem(const std::string& label, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();
```

**Retorna:** `BeginContextMenu` retorna `true` si el menÃº estÃ¡ abierto; `ContextMenuItem` retorna `true` cuando se hace click

### Modal
Crea una ventana modal/diÃ¡logo.

```cpp
bool BeginModal(const std::string& id,
                const std::string& title,
                bool* open,
                const Vec2& size = Vec2(400, 300));

void EndModal();
```

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `title`: TÃ­tulo de la ventana
- `open`: Puntero al estado abierto/cerrado
- `size`: TamaÃ±o de la ventana

**CaracterÃ­sticas:**
- Backdrop oscuro
- Arrastrable por el tÃ­tulo
- BotÃ³n de cerrar (X)
- Se cierra con Escape

## Listas

### ListView
Crea una lista de items seleccionables. Soporta selecciÃ³n simple y mÃºltiple.

```cpp
// SelecciÃ³n simple
bool BeginListView(const std::string& id,
                   const Vec2& size,
                   int* selectedItem,
                   const std::vector<std::string>& items,
                   std::optional<Vec2> pos = std::nullopt);

// SelecciÃ³n mÃºltiple (Ctrl+Click, Shift+Click)
bool BeginListView(const std::string& id,
                   const Vec2& size,
                   std::vector<int>* selectedItems,
                   const std::vector<std::string>& items,
                   std::optional<Vec2> pos = std::nullopt);

void EndListView();
```

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `size`: TamaÃ±o de la lista
- `selectedItem`: Puntero al Ã­ndice seleccionado (selecciÃ³n simple)
- `selectedItems`: Puntero al vector de Ã­ndices seleccionados (selecciÃ³n mÃºltiple)
- `items`: Lista de textos a mostrar
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

## TreeView

### TreeNodeData
Struct auxiliar para construcciÃ³n declarativa de Ã¡rboles.

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
void TreeNodePop();   // DespuÃ©s de mostrar hijos (reducir profundidad)

void EndTreeView();
```

**ParÃ¡metros de TreeNode:**
- `id`: Identificador Ãºnico del nodo
- `label`: Texto del nodo
- `isOpen`: Puntero al estado expandido (pasar no-null para nodos expandibles)
- `isSelected`: Puntero al estado de selecciÃ³n

**Retorna:** `true` si el nodo estÃ¡ expandido y tiene hijos

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

## MenÃº

### MenuBar
Crea una barra de menÃº horizontal.

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

**ParÃ¡metros:**
- `text`: Texto inicial a mostrar en la barra de estado

## Grid

Layout de cuadrÃ­cula con nÃºmero fijo de columnas.

```cpp
void BeginGrid(const std::string& id,
               int columns,
               float rowHeight = 0.0f);

void GridNextCell();
void EndGrid();
```

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `columns`: NÃºmero de columnas
- `rowHeight`: Altura de cada fila (0 = automÃ¡tico)

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
    float minWidth = 40.0f;   // Ancho mÃ­nimo
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

**ParÃ¡metros:**
- `label`: Etiqueta del widget
- `value`: Puntero al color
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

**Retorna:** `true` si el color cambiÃ³

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

**ParÃ¡metros:**
- `id`: Identificador Ãºnico
- `textureHandle`: Handle de textura del backend (ej. `GLuint` casteado a `void*`)
- `size`: TamaÃ±o de la imagen en pÃ­xeles
- `uv0` / `uv1`: Coordenadas UV para sub-regiones de la textura
- `pos`: PosiciÃ³n absoluta (`std::nullopt` para usar layout)

## Espaciado y Cursor

### Spacing
Agrega espaciado vertical.

```cpp
void Spacing(float pixels);
```

### SameLine
ContinÃºa renderizando en la misma lÃ­nea horizontal.

```cpp
void SameLine(float offset = 0.0f);
```

**ParÃ¡metros:**
- `offset`: Offset horizontal adicional en pÃ­xeles

## DPI

### GetDPIScale
Obtiene el factor de escala actual del display (1.0 = 100%).

```cpp
float GetDPIScale();
```

### Scaled
Escala un valor en pÃ­xeles por el factor DPI actual.

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
Button("BotÃ³n personalizado");
PopButtonStyle();
```

## Accesibilidad

### DrawAccessibilityFocusRing
Dibuja un anillo de foco de 2px alrededor de un widget para navegaciÃ³n por teclado.

```cpp
void DrawAccessibilityFocusRing(const Vec2& pos, const Vec2& size);
```

**ParÃ¡metros:**
- `pos`: PosiciÃ³n del widget
- `size`: TamaÃ±o del widget

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

### Estilos TipogrÃ¡ficos
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
    // AcciÃ³n cuando el widget se activa
});
```

### Registrar Callback de Cambio de Valor
```cpp
ctx->RegisterValueChanged("mySlider",
    [](const std::string& id, void* value) {
        float* fval = static_cast<float*>(value);
        // AcciÃ³n cuando el valor cambia
    });
```

## Iconos en Widgets

FluentUI viene con la fuente [Lucide](https://lucide.dev) preempaquetada y
**la carga automÃ¡ticamente al construir `FluentApp`**. No necesitas tocar
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
generado a partir del `.ttf` real, asÃ­ que se convierten implÃ­citamente al
codepoint que esperan los overloads de los widgets. El catÃ¡logo completo
(~1500 iconos) vive en `include/UI/Icons.h`.

### 1. CÃ³mo encuentra la fuente

Al construir un `FluentApp`, la librerÃ­a busca `lucide.ttf` en este orden:

1. `AppConfig::iconFontPath` si lo proporcionas explÃ­citamente.
2. `<directorio del ejecutable>/assets/fonts/lucide.ttf` (ruta canÃ³nica vendorizada).
3. `<cwd>/assets/fonts/lucide.ttf`.
4. `$FLUENTUI_ASSETS_DIR/fonts/lucide.ttf`.

Si nada existe, la app sigue corriendo (los iconos quedan invisibles, no
crashea) y se loguea un warning. CMake copia la fuente al directorio del
binario automÃ¡ticamente, asÃ­ que en builds estÃ¡ndar nunca tienes que
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

El catÃ¡logo `Icons::*` solo es vÃ¡lido para el `.ttf` de Lucide vendorizado â€”
si lo reemplazas, define tus propios constantes o regenera el header con
`tools/gen_icons.py` apuntando a tu nueva fuente.

### 2. API expuesta

Cada widget icon-aware tiene un overload con `uint32_t iconCodepoint` (0 = sin
icono). El icono no afecta los IDs ni la lÃ³gica del widget â€” solo se dibuja.

| Widget | PosiciÃ³n del icono | Notas |
|---|---|---|
| `Button(label, iconCp, â€¦)` | Izquierda del texto | Pasa `label=""` â†’ botÃ³n cuadrado solo-icono |
| `Button(label, iconCp, ButtonSize, â€¦)` | Izquierda | Variante S/M/L |
| `IconButton(iconCp, size?, â€¦)` | Centrado | Atajo idiomÃ¡tico para toolbars |
| `Label(text, iconCp, â€¦)` | Izquierda del texto | |
| `IconLabel(iconCp, size?, color?, â€¦)` | â€” | Icono suelto, sin texto |
| `Checkbox(label, iconCp, â€¦)` | DespuÃ©s del label | Ãštil para badges |
| `RadioButton(label, iconCp, â€¦)` | DespuÃ©s del label | |
| `MenuItem(label, iconCp, â€¦)` | Izquierda | Paridad con `BeginMenu` |
| `BeginMenu(label, iconCp, â€¦)` | Izquierda | Ya existÃ­a en versiones previas |
| `ContextMenuItem(label, iconCp, â€¦)` | Izquierda | |
| `BeginPanel(id, iconCp, â€¦)` | En la barra de tÃ­tulo | |
| `BeginModal(id, title, iconCp, â€¦)` | En el header | Ideal para warning/info/error |
| `BeginTabView(id, ptr, vector<pair<str,cp>>, â€¦)` | Por tab | Pasa `(label, codepoint)` por tab |
| `ComboBox(label, ptr, vector<pair<str,cp>>, â€¦)` | Izquierda del item (dropdown + campo) | TambiÃ©n aplica a `ComboBoxSearchable` |
| `SegmentedControl(id, vector<pair<str,cp>>, â€¦)` | Por segmento | `label=""` â†’ segmento cuadrado |
| `BeginListView(â€¦, vector<pair<str,cp>>, â€¦)` | Por fila | Single y multi-selecciÃ³n |
| `TableColumn{header, â€¦, iconCodepoint}` | En el header de la columna | Campo en la struct |
| `TreeNode(id, label, iconCp, â€¦)` | DespuÃ©s del chevron | |
| `TreeNodeMulti(id, label, iconCp, â€¦)` | DespuÃ©s del chevron | |

Widgets sin overload (por diseÃ±o): `SliderFloat/Int`, `DragFloat/Int/3`,
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

### CatÃ¡logo `Icons::*`

El header `include/UI/Icons.h` se genera automÃ¡ticamente desde el `.ttf`
vendorizado y contiene un `enum Lucide : uint32_t` con TODOS los iconos
(actualmente ~1500). Como es un `enum` no-scoped, **se convierte implÃ­citamente
a `uint32_t`** y entra en cualquier overload sin `static_cast`:

```cpp
Button("Save", Icons::Save);
TreeNode("a", "Assets", Icons::Folder, &open);
TableColumn{"Name", 160, 40, true, Icons::FileText};
```

**Nombres canÃ³nicos** â€” equivalentes a los que ves en
[lucide.dev](https://lucide.dev), convertidos de kebab-case a PascalCase:
`mouse-pointer` â†’ `MousePointer`, `arrow-up-right` â†’ `ArrowUpRight`,
`rotate-cw` â†’ `RotateCw`.

**Aliases semÃ¡nticos** â€” algunos nombres mÃ¡s cortos para el dÃ­a a dÃ­a en un
editor:

| Alias | Equivale a | Uso tÃ­pico |
|---|---|---|
| `Icons::Pointer` | `MousePointer` | SelecciÃ³n |
| `Icons::Stop` | `Square` | Detener reproducciÃ³n |
| `Icons::Cut` | `Scissors` | Edit menu |
| `Icons::Paste` | `Clipboard` | Edit menu |
| `Icons::Open` | `FolderOpen` | File menu |
| `Icons::New` | `FilePlus` | File menu |
| `Icons::Close` | `X` | BotÃ³n de cerrar |
| `Icons::Help` | `CircleHelp` | Tooltips, F1 |
| `Icons::Cube` | `Box` | Mesh, geometrÃ­a |
| `Icons::Record` | `Circle` | Captura |
| `Icons::Rotate` | `RotateCw` | Tool gizmo |
| `Icons::Unlock` | `LockOpen` | Toggle de bloqueo |

Si necesitas algÃºn icono que no encuentras: abre `include/UI/Icons.h` y busca
en el enum (autocompletado del IDE en `Icons::` tambiÃ©n funciona). CategorÃ­as
tÃ­picas: archivos (`Folder*`, `File*`, `Save`, `Download`), ediciÃ³n (`Copy`,
`Pencil`, `Trash`, `Scissors`), transform (`Move`, `RotateCw`, `Scale`,
`Box`), reproducciÃ³n (`Play`, `Pause`, `Square`), estado (`Check`, `X`,
`Info`, `TriangleAlert`, `CircleHelp`), navegaciÃ³n (`Chevron*`, `Arrow*`,
`House`), sistema (`Settings`, `Terminal`, `Monitor`, `Cpu`, `Database`).

### Regenerar el catÃ¡logo

Para actualizar la versiÃ³n de Lucide:

```bash
# 1) Reemplaza assets/fonts/lucide.ttf con la versiÃ³n nueva
# 2) Regenera el header
pip install fonttools
python tools/gen_icons.py
```

El script lee el `.ttf` directamente con `fontTools` y reescribe
`include/UI/Icons.h` con los codepoints correctos. Commitea ambos archivos.

