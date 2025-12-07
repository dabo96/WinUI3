# FluentGUI - Referencia de API

## Índice
1. [Inicialización](#inicialización)
2. [Widgets Básicos](#widgets-básicos)
3. [Contenedores](#contenedores)
4. [Layout](#layout)
5. [Temas y Estilos](#temas-y-estilos)
6. [Callbacks y Eventos](#callbacks-y-eventos)

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

### DestroyContext
Destruye el contexto UI y libera recursos.

```cpp
void DestroyContext();
```

## Widgets Básicos

### Button
Crea un botón clickeable.

```cpp
bool Button(const std::string& label, 
            const Vec2& size = Vec2(0, 0), 
            const Vec2& pos = Vec2(0, 0), 
            bool enabled = true);
```

**Parámetros:**
- `label`: Texto del botón
- `size`: Tamaño del botón (Vec2(0,0) para auto-sizing)
- `pos`: Posición absoluta (Vec2(0,0) para usar layout)
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
- `position`: Posición opcional (std::nullopt para usar layout)
- `variant`: Estilo tipográfico (Caption, Body, Subtitle, Title, etc.)
- `disabled`: Si el texto está deshabilitado

### Checkbox
Crea una casilla de verificación.

```cpp
bool Checkbox(const std::string& label, bool* value = nullptr);
```

**Parámetros:**
- `label`: Etiqueta del checkbox
- `value`: Puntero al valor booleano (nullptr para solo visualización)

**Retorna:** `true` si el valor cambió

### RadioButton
Crea un botón de opción única.

```cpp
bool RadioButton(const std::string& label, 
                 int* value, 
                 int optionValue, 
                 const std::string& group = "");
```

**Parámetros:**
- `label`: Etiqueta del radio button
- `value`: Puntero al valor actual del grupo
- `optionValue`: Valor que representa este radio button
- `group`: ID del grupo (radio buttons con el mismo group son mutuamente exclusivos)

**Retorna:** `true` si se seleccionó este radio button

### SliderFloat / SliderInt
Crea un deslizador para valores numéricos.

```cpp
bool SliderFloat(const std::string& label, 
                 float* value, 
                 float minValue, 
                 float maxValue, 
                 float width = 200.0f, 
                 const char* format = "%.2f");

bool SliderInt(const std::string& label, 
               int* value, 
               int minValue, 
               int maxValue, 
               float width = 200.0f);
```

**Retorna:** `true` si el valor cambió

### TextInput
Crea un campo de texto editable.

```cpp
bool TextInput(const std::string& label, 
               std::string* value, 
               float width = 200.0f, 
               bool multiline = false);
```

**Retorna:** `true` si el texto cambió

### ProgressBar
Muestra una barra de progreso.

```cpp
void ProgressBar(float fraction, 
                 const Vec2& size = Vec2(0, 0), 
                 const std::string& overlay = "");
```

**Parámetros:**
- `fraction`: Valor entre 0.0 y 1.0
- `size`: Tamaño de la barra
- `overlay`: Texto opcional a mostrar sobre la barra

## Contenedores

### Panel
Crea un panel con título y opción de minimizar.

```cpp
bool BeginPanel(const std::string& id, 
                const Vec2& size = Vec2(0, 0), 
                bool reserveLayoutSpace = true,
                std::optional<bool> useAcrylic = std::nullopt, 
                std::optional<float> acrylicOpacity = std::nullopt);

void EndPanel();
```

**Características:**
- Título clickeable para minimizar/expandir
- Efecto acrylic opcional
- Soporte para widgets dentro del panel

### ScrollView
Crea una vista con scrollbars.

```cpp
bool BeginScrollView(const std::string& id, 
                     const Vec2& size, 
                     Vec2* scrollOffset = nullptr);

void EndScrollView();
```

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
                  const Vec2& size = Vec2(0, 0));

void EndTabView();
```

### ListView
Crea una lista de items seleccionables.

```cpp
bool BeginListView(const std::string& id, 
                   const Vec2& size, 
                   int* selectedItem, 
                   const std::vector<std::string>& items);

void EndListView();
```

### TreeView
Crea una vista de árbol con nodos expandibles.

```cpp
bool BeginTreeView(const std::string& id, const Vec2& size);

bool TreeNode(const std::string& id, 
              const std::string& label, 
              bool* isOpen = nullptr, 
              bool* isSelected = nullptr);

void TreeNodePush(); // Antes de mostrar hijos
void TreeNodePop();  // Después de mostrar hijos

void EndTreeView();
```

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

### Modal
Crea una ventana modal/diálogo.

```cpp
bool BeginModal(const std::string& id, 
                const std::string& title, 
                bool* open, 
                const Vec2& size = Vec2(400, 300));

void EndModal();
```

**Características:**
- Backdrop oscuro
- Arrastrable por el título
- Botón de cerrar (X)
- Se cierra con Escape

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
    }
    EndMenu();
}
EndMenuBar();
```

### ContextMenu
Crea un menú contextual (clic derecho).

```cpp
bool BeginContextMenu(const std::string& id);
bool ContextMenuItem(const std::string& label, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();
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

### BeginHorizontal / EndHorizontal
Crea un layout horizontal (los widgets se apilan horizontalmente).

```cpp
void BeginHorizontal(float spacing = -1.0f, 
                     std::optional<Vec2> size = std::nullopt, 
                     std::optional<Vec2> padding = std::nullopt);

void EndHorizontal(bool advanceParent = true);
```

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

