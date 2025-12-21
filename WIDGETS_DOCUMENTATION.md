# FluentGUI - Documentación de Widgets

Esta documentación proporciona ejemplos de uso para todos los widgets disponibles en FluentGUI.

## Tabla de Contenidos

1. [Configuración Inicial](#configuración-inicial)
2. [Layout Helpers](#layout-helpers)
3. [Widgets Básicos](#widgets-básicos)
4. [Controles de Entrada](#controles-de-entrada)
5. [Contenedores](#contenedores)
6. [Widgets Especializados](#widgets-especializados)
7. [Widgets Avanzados](#widgets-avanzados)
8. [Sistema de Docking](#sistema-de-docking)

---

## Configuración Inicial

Antes de usar cualquier widget, necesitas inicializar el contexto de FluentGUI:

```cpp
#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include "core/Context.h"
#include <SDL3/SDL.h>

using namespace FluentUI;

// En tu función de inicialización
SDL_Window* window = SDL_CreateWindow("Mi App", 1280, 720, SDL_WINDOW_OPENGL);
UIContext* ctx = CreateContext(window);
ctx->style = GetDarkFluentStyle(); // o GetDefaultFluentStyle() para tema claro

// En tu loop principal
void Update(float deltaTime) {
    NewFrame(deltaTime);
    
    // Aquí va tu código de UI
    
    Render();
}
```

---

## Layout Helpers

### BeginVertical / EndVertical

Crea un layout vertical que organiza los widgets de arriba hacia abajo.

```cpp
BeginVertical(8.0f); // spacing entre elementos
    Label("Primer elemento");
    Label("Segundo elemento");
    Button("Botón");
EndVertical();
```

**Parámetros:**
- `spacing`: Espaciado entre elementos (default: -1.0f usa el spacing del estilo)
- `size`: Tamaño opcional del contenedor
- `padding`: Padding opcional

### BeginHorizontal / EndHorizontal

Crea un layout horizontal que organiza los widgets de izquierda a derecha.

```cpp
BeginHorizontal(10.0f);
    Button("Izquierda");
    Button("Centro");
    Button("Derecha");
EndHorizontal();
```

### Spacing

Añade espacio vertical entre widgets.

```cpp
Label("Texto 1");
Spacing(20.0f); // 20 píxeles de espacio
Label("Texto 2");
```

### SameLine

Coloca el siguiente widget en la misma línea que el anterior.

```cpp
Button("Botón 1");
SameLine(10.0f); // Offset de 10 píxeles desde el botón anterior
Button("Botón 2");
```

---

## Widgets Básicos

### Button

Crea un botón clickeable.

```cpp
bool clicked = Button("Click Me");
if (clicked) {
    std::cout << "Botón presionado!" << std::endl;
}

// Con tamaño personalizado
Button("Botón Grande", Vec2(200, 50));

// Botón deshabilitado
Button("Deshabilitado", Vec2(120, 32), Vec2(0, 0), false);

// Botón en posición absoluta
Button("Posición Fija", Vec2(100, 30), Vec2(50, 50));
```

**Parámetros:**
- `label`: Texto del botón
- `size`: Tamaño del botón (Vec2(0,0) = auto)
- `pos`: Posición absoluta (Vec2(0,0) = layout)
- `enabled`: Si el botón está habilitado (default: true)

**Retorna:** `true` si el botón fue clickeado en este frame

### Label

Muestra texto estático.

```cpp
// Label básico
Label("Texto simple");

// Con variante de tipografía
Label("Título", std::nullopt, TypographyStyle::Title);
Label("Subtítulo", std::nullopt, TypographyStyle::Subtitle);
Label("Cuerpo", std::nullopt, TypographyStyle::Body);
Label("Caption", std::nullopt, TypographyStyle::Caption);

// Label deshabilitado
Label("Texto deshabilitado", std::nullopt, TypographyStyle::Body, true);

// En posición absoluta
Label("Posición fija", Vec2(100, 100));
```

**Parámetros:**
- `text`: Texto a mostrar
- `position`: Posición opcional (std::nullopt = layout)
- `variant`: Estilo de tipografía (default: Body)
- `disabled`: Si está deshabilitado (default: false)

### Separator

Dibuja una línea separadora horizontal.

```cpp
Label("Sección 1");
Separator();
Label("Sección 2");
```

---

## Controles de Entrada

### Checkbox

Crea una casilla de verificación.

```cpp
bool isChecked = false;
if (Checkbox("Habilitar opción", &isChecked)) {
    std::cout << "Checkbox cambió a: " << (isChecked ? "ON" : "OFF") << std::endl;
}
```

**Parámetros:**
- `label`: Texto del checkbox
- `value`: Puntero al valor booleano
- `pos`: Posición absoluta opcional

**Retorna:** `true` si el valor cambió en este frame

### RadioButton

Crea un botón de radio para selección única.

```cpp
int selectedOption = 0;
if (RadioButton("Opción 1", &selectedOption, 0)) {
    std::cout << "Seleccionada opción 1" << std::endl;
}
if (RadioButton("Opción 2", &selectedOption, 1)) {
    std::cout << "Seleccionada opción 2" << std::endl;
}
if (RadioButton("Opción 3", &selectedOption, 2)) {
    std::cout << "Seleccionada opción 3" << std::endl;
}
```

**Parámetros:**
- `label`: Texto del radio button
- `value`: Puntero al valor entero que almacena la opción seleccionada
- `optionValue`: Valor que representa esta opción
- `group`: Grupo opcional (para múltiples grupos)
- `pos`: Posición absoluta opcional

**Retorna:** `true` si esta opción fue seleccionada

### ToggleSwitch

Crea un interruptor tipo switch.

```cpp
bool isEnabled = false;
if (ToggleSwitch("Modo oscuro", &isEnabled)) {
    // Cambiar tema
    ctx->style = isEnabled ? GetDarkFluentStyle() : GetDefaultFluentStyle();
}
```

### TextInput

Crea un campo de entrada de texto.

```cpp
std::string userName = "Usuario";
if (TextInput("Nombre", &userName, 250.0f)) {
    std::cout << "Texto cambiado: " << userName << std::endl;
}

// Multilínea
std::string description = "";
TextInput("Descripción", &description, 300.0f, true);
```

**Parámetros:**
- `label`: Etiqueta del campo
- `value`: Puntero al string que almacena el texto
- `width`: Ancho del campo
- `multiline`: Si es multilínea (default: false)
- `pos`: Posición absoluta opcional

**Retorna:** `true` si el texto cambió

### SliderFloat

Crea un deslizador para valores flotantes.

```cpp
float speed = 0.5f;
if (SliderFloat("Velocidad", &speed, 0.0f, 1.0f, 250.0f)) {
    std::cout << "Velocidad: " << speed << std::endl;
}

// Con formato personalizado
float value = 50.0f;
SliderFloat("Porcentaje", &value, 0.0f, 100.0f, 200.0f, "%.1f%%");
```

**Parámetros:**
- `label`: Etiqueta del slider
- `value`: Puntero al valor flotante
- `minValue`: Valor mínimo
- `maxValue`: Valor máximo
- `width`: Ancho del slider
- `format`: Formato de visualización (default: "%.2f")
- `pos`: Posición absoluta opcional

**Retorna:** `true` si el valor cambió

### SliderInt

Crea un deslizador para valores enteros.

```cpp
int iterations = 5;
if (SliderInt("Iteraciones", &iterations, 1, 20, 250.0f)) {
    std::cout << "Iteraciones: " << iterations << std::endl;
}
```

**Parámetros:**
- `label`: Etiqueta del slider
- `value`: Puntero al valor entero
- `minValue`: Valor mínimo
- `maxValue`: Valor máximo
- `width`: Ancho del slider
- `pos`: Posición absoluta opcional

### SpinBox

Crea un control de entrada numérica con botones incremento/decremento.

```cpp
int count = 10;
if (SpinBox("Cantidad", &count, 0, 100, 150.0f)) {
    std::cout << "Cantidad: " << count << std::endl;
}
```

### ComboBox

Crea un menú desplegable de selección.

```cpp
int selectedIndex = 0;
std::vector<std::string> items = {"Opción 1", "Opción 2", "Opción 3", "Opción 4"};
if (ComboBox("Seleccionar", &selectedIndex, items, 200.0f)) {
    std::cout << "Seleccionado: " << items[selectedIndex] << std::endl;
}
```

**Parámetros:**
- `label`: Etiqueta del combo box
- `currentItem`: Puntero al índice del item seleccionado
- `items`: Vector de strings con las opciones
- `width`: Ancho del combo box
- `pos`: Posición absoluta opcional

**Retorna:** `true` si la selección cambió

### ProgressBar

Muestra una barra de progreso.

```cpp
float progress = 0.75f; // 75%
ProgressBar(progress, Vec2(300, 20), "Cargando... 75%");

// Sin overlay
ProgressBar(progress, Vec2(300, 20));
```

**Parámetros:**
- `fraction`: Valor entre 0.0 y 1.0
- `size`: Tamaño de la barra (Vec2(0,0) = auto)
- `overlay`: Texto opcional sobre la barra
- `pos`: Posición absoluta opcional

---

## Contenedores

### Panel

Crea un panel contenedor con fondo y borde. Los paneles pueden ser dockeables.

```cpp
// Panel básico
if (BeginPanel("MiPanel", Vec2(300, 200))) {
    Label("Contenido del panel");
    Button("Botón dentro del panel");
}
EndPanel();

// Panel con efecto Acrylic
if (BeginPanel("PanelAcrylic", Vec2(300, 200), true, true, 0.85f)) {
    Label("Panel con efecto Acrylic");
}
EndPanel();

// Panel en posición absoluta
if (BeginPanel("PanelFijo", Vec2(250, 150), true, false, std::nullopt, Vec2(50, 50))) {
    Label("Panel en posición fija");
}
EndPanel();

// Panel no dockeable
if (BeginPanel("PanelFijo", Vec2(250, 150), true, false, std::nullopt, Vec2(0, 0), false)) {
    Label("Este panel no se puede dockear");
}
EndPanel();
```

**Parámetros de BeginPanel:**
- `id`: Identificador único del panel
- `size`: Tamaño del panel (Vec2(0,0) = auto)
- `reserveLayoutSpace`: Si reserva espacio en el layout (default: true)
- `useAcrylic`: Si usa efecto Acrylic (std::nullopt = usa estilo por defecto)
- `acrylicOpacity`: Opacidad del efecto Acrylic (0.0-1.0)
- `pos`: Posición absoluta (Vec2(0,0) = layout)
- `isDockable`: Si el panel puede ser dockeado (default: true)

**Retorna:** `true` si el panel está visible (no minimizado o en tab inactivo)

### ScrollView

Crea un área con scroll para contenido que excede el tamaño visible.

```cpp
Vec2 scrollOffset(0, 0);
if (BeginScrollView("MiScroll", Vec2(300, 200), &scrollOffset)) {
    for (int i = 0; i < 50; i++) {
        Label("Item " + std::to_string(i));
        Spacing(5);
    }
}
EndScrollView();
```

**Parámetros de BeginScrollView:**
- `id`: Identificador único
- `size`: Tamaño del área visible
- `scrollOffset`: Puntero al offset de scroll (se actualiza automáticamente)
- `pos`: Posición absoluta opcional

### TabView

Crea un sistema de pestañas.

```cpp
int activeTab = 0;
std::vector<std::string> tabs = {"Tab 1", "Tab 2", "Tab 3"};
if (BeginTabView("MiTabView", &activeTab, tabs, Vec2(500, 400))) {
    if (activeTab == 0) {
        Label("Contenido de Tab 1");
        Button("Botón en Tab 1");
    } else if (activeTab == 1) {
        Label("Contenido de Tab 2");
    } else if (activeTab == 2) {
        Label("Contenido de Tab 3");
    }
}
EndTabView();
```

**Parámetros de BeginTabView:**
- `id`: Identificador único
- `activeTab`: Puntero al índice de la pestaña activa
- `tabLabels`: Vector de strings con los nombres de las pestañas
- `size`: Tamaño del TabView (Vec2(0,0) = auto)
- `pos`: Posición absoluta opcional

### ListView

Crea una lista seleccionable de items.

```cpp
int selectedItem = -1;
std::vector<std::string> items = {"Item A", "Item B", "Item C", "Item D"};
if (BeginListView("MiLista", Vec2(300, 200), &selectedItem, items)) {
    // El contenido se maneja automáticamente
}
EndListView();

if (selectedItem >= 0) {
    Label("Seleccionado: " + items[selectedItem]);
}
```

**Parámetros de BeginListView:**
- `id`: Identificador único
- `size`: Tamaño de la lista
- `selectedItem`: Puntero al índice del item seleccionado (-1 = ninguno)
- `items`: Vector de strings con los items
- `pos`: Posición absoluta opcional

### TreeView

Crea una vista de árbol jerárquica.

```cpp
bool node1Open = false;
bool node2Open = false;
bool node3Open = false;

if (BeginTreeView("MiArbol", Vec2(300, 400))) {
    if (TreeNode("Node1", "Nodo 1", &node1Open)) {
        TreeNodePush();
        Label("Hijo 1.1");
        Label("Hijo 1.2");
        TreeNodePop();
    }
    
    if (TreeNode("Node2", "Nodo 2", &node2Open)) {
        TreeNodePush();
        Label("Hijo 2.1");
        if (TreeNode("Node3", "Hijo 2.2", &node3Open)) {
            TreeNodePush();
            Label("Nieto 2.2.1");
            TreeNodePop();
        }
        TreeNodePop();
    }
    
    Label("Nodo 3 (sin hijos)");
}
EndTreeView();
```

**Uso de TreeNode:**
- `TreeNode()` retorna `true` si el nodo está expandido
- `TreeNodePush()` incrementa la indentación para los hijos
- `TreeNodePop()` decrementa la indentación después de los hijos

**Parámetros de TreeNode:**
- `id`: Identificador único del nodo
- `label`: Texto del nodo
- `isOpen`: Puntero al estado de expansión (nullptr = no expandible)
- `isSelected`: Puntero al estado de selección (nullptr = no seleccionable)

---

## Widgets Especializados

### Tooltip

Muestra un tooltip cuando el mouse está sobre el widget anterior.

```cpp
Button("Hover me");
Tooltip("Este es un tooltip informativo", 0.5f); // Delay de 0.5 segundos
```

**Parámetros:**
- `text`: Texto del tooltip
- `delay`: Delay en segundos antes de mostrar (default: 0.5f)

### Context Menu

Crea un menú contextual que aparece al hacer click derecho.

```cpp
if (BeginPanel("ContextPanel", Vec2(300, 200))) {
    Label("Click derecho aquí");
}
EndPanel();

if (BeginContextMenu("MiContextMenu")) {
    if (ContextMenuItem("Copiar")) {
        std::cout << "Copiar seleccionado" << std::endl;
    }
    if (ContextMenuItem("Pegar")) {
        std::cout << "Pegar seleccionado" << std::endl;
    }
    ContextMenuSeparator();
    if (ContextMenuItem("Eliminar", false)) { // Deshabilitado
        std::cout << "Eliminar seleccionado" << std::endl;
    }
}
EndContextMenu();
```

**Parámetros de ContextMenuItem:**
- `label`: Texto del item
- `enabled`: Si está habilitado (default: true)

**Retorna:** `true` si el item fue clickeado

### Modal

Crea un diálogo modal.

```cpp
bool showModal = false;

// Botón para abrir el modal
if (Button("Mostrar Modal")) {
    showModal = true;
}

if (BeginModal("MiModal", "Confirmar Acción", &showModal, Vec2(400, 200))) {
    Label("¿Estás seguro de realizar esta acción?");
    Spacing(20);
    
    BeginHorizontal();
    if (Button("OK", Vec2(100, 32))) {
        showModal = false;
        std::cout << "Acción confirmada" << std::endl;
    }
    SameLine(10.0f);
    if (Button("Cancelar", Vec2(100, 32))) {
        showModal = false;
    }
    EndHorizontal();
}
EndModal();
```

**Parámetros de BeginModal:**
- `id`: Identificador único
- `title`: Título del modal
- `open`: Puntero al estado de visibilidad
- `size`: Tamaño del modal (default: Vec2(400, 300))

---

## Widgets Avanzados

### DragFloat

Crea un control de arrastre para valores flotantes (útil para editores).

```cpp
float positionX = 0.0f;
if (DragFloat("Posición X", &positionX, 1.0f, -100.0f, 100.0f, 150.0f)) {
    std::cout << "X: " << positionX << std::endl;
}
```

**Parámetros:**
- `label`: Etiqueta
- `value`: Puntero al valor
- `speed`: Velocidad de cambio al arrastrar
- `min`: Valor mínimo (0.0 = sin límite)
- `max`: Valor máximo (0.0 = sin límite)
- `width`: Ancho del control
- `pos`: Posición absoluta opcional

### DragVector3

Crea un control de arrastre para un vector 3D.

```cpp
float position[3] = {0.0f, 0.0f, 0.0f};
if (DragVector3("Posición", position, 1.0f, -100.0f, 100.0f)) {
    std::cout << "Pos: (" << position[0] << ", " << position[1] << ", " << position[2] << ")" << std::endl;
}
```

### CollapsibleHeader

Crea un encabezado colapsable simple.

```cpp
if (CollapsibleHeader("Opciones Avanzadas", false)) {
    Label("Contenido visible cuando está expandido");
    Checkbox("Opción 1", &option1);
    Checkbox("Opción 2", &option2);
}
```

**Retorna:** `true` si está expandido

### BeginCollapsibleGroup / EndCollapsibleGroup

Similar a CollapsibleHeader pero permite más control.

```cpp
if (BeginCollapsibleGroup("Configuración", true)) {
    Label("Contenido del grupo");
    SliderFloat("Valor", &value, 0.0f, 1.0f);
}
EndCollapsibleGroup();
```

### Splitter

Crea un divisor redimensionable entre dos áreas.

```cpp
float size1 = 200.0f;
float size2 = 300.0f;
if (Splitter("MiSplitter", true, 4.0f, &size1, &size2, 50.0f, 50.0f)) {
    // Los tamaños se actualizan automáticamente
}
```

**Parámetros:**
- `id`: Identificador único
- `splitVertically`: Si divide verticalmente (false = horizontal)
- `thickness`: Grosor del divisor
- `size1`: Puntero al tamaño del primer panel
- `size2`: Puntero al tamaño del segundo panel
- `minSize1`: Tamaño mínimo del primer panel
- `minSize2`: Tamaño mínimo del segundo panel

### ColorPicker

Crea un selector de color.

```cpp
Color myColor(1.0f, 0.5f, 0.0f, 1.0f);
if (ColorPicker("Color", &myColor)) {
    std::cout << "Color cambiado" << std::endl;
}
```

### Image

Muestra una imagen/textura.

```cpp
uint32_t textureId = LoadTexture("imagen.png");
Image(textureId, Vec2(200, 200));

// Con UVs personalizados (para sprites)
Image(textureId, Vec2(100, 100), Vec2(0, 0), Vec2(0.5f, 0.5f));

// Con tinte de color
Image(textureId, Vec2(200, 200), Vec2(0, 0), Vec2(1, 1), Color(1.0f, 0.8f, 0.8f, 1.0f));
```

---

## Sistema de Docking

FluentGUI incluye un sistema completo de docking que permite a los paneles ser organizados y reorganizados dinámicamente.

### BeginDockSpace / EndDockSpace

Inicia y termina un espacio de docking. Todos los paneles dockeables deben estar dentro de este espacio.

```cpp
BeginDockSpace("MainDockSpace");

// Los paneles dockeables se pueden arrastrar y dockear aquí
if (BeginPanel("Panel1", Vec2(300, 400), true, false, std::nullopt, Vec2(0, 0), true)) {
    Label("Este panel es dockeable");
}
EndPanel();

if (BeginPanel("Panel2", Vec2(300, 400), true, false, std::nullopt, Vec2(320, 0), true)) {
    Label("Este también es dockeable");
}
EndPanel();

EndDockSpace();
```

**Características del Docking:**
- Los paneles pueden ser arrastrados desde su barra de título
- Se pueden dockear en los bordes (izquierda, derecha, arriba, abajo) o como pestañas
- El espacio disponible se divide automáticamente según el número de paneles libres
- Los paneles pueden ser desdokeados haciendo click en el botón de undock

**Notas:**
- Solo los paneles con `isDockable = true` pueden ser dockeados
- Los paneles dockeados no tienen barra de título individual (se muestran como pestañas)
- El sistema calcula automáticamente las zonas de docking según los paneles disponibles

---

## MenuBar

Crea una barra de menú en la parte superior.

```cpp
if (BeginMenuBar()) {
    if (BeginMenu("Archivo")) {
        if (MenuItem("Nuevo")) {
            std::cout << "Nuevo archivo" << std::endl;
        }
        if (MenuItem("Abrir")) {
            std::cout << "Abrir archivo" << std::endl;
        }
        MenuSeparator();
        if (MenuItem("Salir")) {
            running = false;
        }
        EndMenu();
    }
    
    if (BeginMenu("Editar")) {
        if (MenuItem("Copiar")) {
            std::cout << "Copiar" << std::endl;
        }
        if (MenuItem("Pegar", false)) { // Deshabilitado
            std::cout << "Pegar" << std::endl;
        }
        EndMenu();
    }
    
    EndMenuBar();
}
```

---

## Notificaciones Toast

Muestra notificaciones temporales en la esquina de la pantalla.

```cpp
// Mostrar notificación
ShowNotification("Operación completada", ToastType::Success);
ShowNotification("Error al guardar", ToastType::Error);
ShowNotification("Información importante", ToastType::Info);
ShowNotification("Advertencia", ToastType::Warning);

// Renderizar (llamar al final del frame)
RenderNotifications();
```

**Tipos de Toast:**
- `ToastType::Info` - Información (azul)
- `ToastType::Success` - Éxito (verde)
- `ToastType::Warning` - Advertencia (amarillo)
- `ToastType::Error` - Error (rojo)

---

## Ejemplo Completo

```cpp
#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include "core/Context.h"
#include <SDL3/SDL.h>

using namespace FluentUI;

int main() {
    SDL_Window* window = SDL_CreateWindow("Mi App", 1280, 720, SDL_WINDOW_OPENGL);
    UIContext* ctx = CreateContext(window);
    ctx->style = GetDarkFluentStyle();
    
    // Variables de estado
    bool running = true;
    std::string userName = "Usuario";
    float sliderValue = 0.5f;
    int selectedTab = 0;
    bool showModal = false;
    
    while (running) {
        // Actualizar input
        ctx->input.Update(window);
        
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            ctx->input.ProcessEvent(e);
        }
        
        // Nuevo frame
        NewFrame(0.016f); // ~60 FPS
        
        // Espacio de docking
        BeginDockSpace("MainDock");
        
        // Panel principal
        if (BeginPanel("MainPanel", Vec2(600, 500), true, false, std::nullopt, Vec2(0, 0), true)) {
            // TabView
            std::vector<std::string> tabs = {"General", "Configuración"};
            if (BeginTabView("MainTabs", &selectedTab, tabs)) {
                if (selectedTab == 0) {
                    Label("Bienvenido", std::nullopt, TypographyStyle::Title);
                    Spacing(10);
                    
                    TextInput("Nombre", &userName, 200.0f);
                    Spacing(5);
                    
                    SliderFloat("Valor", &sliderValue, 0.0f, 1.0f, 250.0f);
                    Spacing(10);
                    
                    if (Button("Mostrar Modal")) {
                        showModal = true;
                    }
                } else if (selectedTab == 1) {
                    Label("Configuración", std::nullopt, TypographyStyle::Title);
                    // Más widgets...
                }
            }
            EndTabView();
        }
        EndPanel();
        
        EndDockSpace();
        
        // Modal
        if (BeginModal("ConfirmModal", "Confirmar", &showModal)) {
            Label("¿Estás seguro?");
            Spacing(20);
            if (Button("OK")) showModal = false;
            SameLine(10.0f);
            if (Button("Cancelar")) showModal = false;
        }
        EndModal();
        
        // Renderizar notificaciones
        RenderNotifications();
        
        // Renderizar frame
        Render();
    }
    
    DestroyContext();
    SDL_DestroyWindow(window);
    return 0;
}
```

---

## Notas Finales

- Todos los widgets que retornan `bool` retornan `true` solo en el frame donde ocurre el evento
- Los widgets con posición `Vec2(0, 0)` usan el layout automático
- Los paneles deben tener IDs únicos
- El sistema de docking funciona mejor cuando los paneles tienen tamaños definidos
- Usa `Spacing()` para añadir espacio entre widgets en layouts verticales
- Usa `SameLine()` para colocar widgets horizontalmente

Para más información, consulta los ejemplos en el directorio `examples/`.

