# FluentUI Quickstart (5 minutos)

Crea una ventana con un botón funcional en menos de 30 líneas.

## Hello, FluentUI

```cpp
#include <FluentUI/API.h>

int main() {
    FluentUI::FluentApp app;
    if (!app.init({"Hello FluentUI", 800, 600})) return 1;

    int counter = 0;
    app.root([&](FluentUI::UIBuilder& ui) {
        ui.panel("main", [&](FluentUI::UIBuilder& u) {
            u.label("Has pulsado " + std::to_string(counter) + " veces");
            if (u.button("Pulsar")) counter++;
        });
    });

    app.run();
    return 0;
}
```

## CMake

```cmake
add_subdirectory(third_party/FluentUI)
target_link_libraries(my_app PRIVATE FluentUI)
```

## Conceptos clave

- **Modo inmediato:** describes la UI cada frame; no hay árbol de objetos.
- **`FluentApp`:** crea ventana SDL + contexto OpenGL + renderer.
- **`UIBuilder`:** API encadenable basada en lambdas para describir la UI.
- **`Style`:** todo el theming es configurable en runtime (ver `ShowStyleEditor`).

## Siguientes pasos

- Lanza `FluentUI::Demo::ShowDemoWindow()` para ver todos los widgets.
- Lanza `FluentUI::Demo::ShowStyleEditor()` para ajustar colores en vivo.
- Pulsa **Ctrl+Shift+P** dentro de tu app para activar el inspector.
- Mira `docs/COOKBOOK.md` para recetas frecuentes (editor de propiedades, plot dashboard, drag-drop).

## Widgets disponibles (resumen)

| Categoría     | Widgets principales |
|---------------|---------------------|
| Básicos       | `Button`, `Label`, `LabelWrapped`, `LabelRich`, `Checkbox`, `RadioButton`, `Separator`, `ProgressBar`, `SegmentedControl` |
| Input         | `TextInput` (multiline + callbacks), `SliderFloat/Int`, `DragFloat/Int`, `ComboBox`, `ComboBoxSearchable`, `ColorPicker` (eyedropper), `DatePicker`, `TimePicker` |
| Listas/Trees  | `BeginListView`, `BeginTreeView`, `TreeNode`, `TreeNodeMulti` (rangeada) |
| Tablas        | `BeginTable` (sort + resize + freeze cols + multi-row select) |
| Plots         | `PlotLines`, `PlotHistogram`, `Sparkline` |
| Contenedores  | `BeginPanel`, `BeginScrollView`, `BeginTabView`, `BeginGrid`, splitters |
| Menús         | `BeginMenuBar`, `BeginMenu`, `MenuItem`, `BeginContextMenu` |
| Overlays      | `Tooltip`, `Modal` |
| Drag-drop     | `DragDropSource<T>`, `DragDropTarget` (tipado, payload binario) |
