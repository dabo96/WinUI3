# FluentGUI - Ejemplos y Documentación

## Ejemplos Disponibles

### main.cpp
Ejemplo básico que demuestra el uso de varios widgets de FluentGUI:
- Button
- Label
- Checkbox
- RadioButton
- Slider
- TextInput
- ComboBox
- Panel
- ScrollView
- TabView
- Tooltip
- ContextMenu
- Modal
- ListView
- TreeView
- MenuBar

## Compilación

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Ejecución

```bash
./FluentGUIExample  # Linux/Mac
FluentGUIExample.exe  # Windows
```

## Widgets Disponibles

### Widgets Básicos
- **Button**: Botón clickeable con estados hover/pressed/disabled
- **Label**: Texto simple con diferentes variantes tipográficas
- **Separator**: Línea separadora horizontal/vertical
- **Checkbox**: Casilla de verificación
- **RadioButton**: Botón de opción única (grupos)
- **Slider**: Deslizador para valores numéricos (float/int)
- **ProgressBar**: Barra de progreso
- **TextInput**: Campo de texto editable

### Contenedores
- **Panel**: Panel con título, minimizable, con efecto acrylic
- **ScrollView**: Vista con scrollbars vertical/horizontal
- **TabView**: Vista con pestañas
- **ListView**: Lista de items seleccionables
- **TreeView**: Vista de árbol con nodos expandibles

### Widgets Especializados
- **Tooltip**: Información contextual al hacer hover
- **ContextMenu**: Menú contextual (clic derecho)
- **Modal**: Ventana modal/diálogo
- **MenuBar**: Barra de menú horizontal con menús desplegables

### Layout
- **BeginVertical/EndVertical**: Layout vertical
- **BeginHorizontal/EndHorizontal**: Layout horizontal
- **Spacing**: Espaciado personalizado
- **SameLine**: Continuar en la misma línea

## API Básica

### Inicialización
```cpp
#include "FluentGUI.h"

auto* ctx = FluentUI::CreateContext(window);
// ... en el loop principal
FluentUI::NewFrame(deltaTime);
// ... tus widgets aquí
FluentUI::Render();
FluentUI::DestroyContext();
```

### Ejemplo de Widgets
```cpp
// Button
if (FluentUI::Button("Click Me")) {
    // Acción al hacer click
}

// Label
FluentUI::Label("Hello World", std::nullopt, FluentUI::TypographyStyle::Title);

// Panel
if (FluentUI::BeginPanel("My Panel", Vec2(300, 200))) {
    FluentUI::Label("Panel Content");
    FluentUI::Button("Button in Panel");
}
FluentUI::EndPanel();
```

## Temas

### Cambiar Tema
```cpp
// Tema oscuro (por defecto)
ctx->style = FluentUI::GetDarkFluentStyle();

// Tema claro
ctx->style = FluentUI::GetDefaultFluentStyle();

// Tema personalizado con color de acento
ctx->style = FluentUI::CreateCustomFluentStyle(
    FluentUI::FluentColors::AccentPurple, // Color de acento
    true // darkTheme
);
```

### Colores de Acento Disponibles
- `FluentColors::AccentBlue` (por defecto)
- `FluentColors::AccentGreen`
- `FluentColors::AccentPurple`
- `FluentColors::AccentOrange`
- `FluentColors::AccentPink`
- `FluentColors::AccentTeal`

## Estilos Tipográficos

- `TypographyStyle::Caption` (12px)
- `TypographyStyle::Body` (14px)
- `TypographyStyle::BodyStrong` (14px Bold)
- `TypographyStyle::Subtitle` (18px)
- `TypographyStyle::SubtitleStrong` (18px Bold)
- `TypographyStyle::Title` (20px)
- `TypographyStyle::TitleLarge` (28px)
- `TypographyStyle::Display` (42px)

## Callbacks y Eventos

```cpp
// Registrar callback para un widget
ctx->RegisterCallback(widgetId, []() {
    // Acción cuando el widget se activa
});

// Registrar callback para cambios de valor
ctx->RegisterValueChanged("mySlider", [](const std::string& id, void* value) {
    float* fval = static_cast<float*>(value);
    // Acción cuando el valor cambia
});
```

