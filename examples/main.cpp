#include <SDL3/SDL.h>
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include "core/Context.h"
#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include <iostream>
using namespace FluentUI;

// Variables globales para el estado de la aplicación
struct DemoState {
    // Widgets básicos
    int clickCount = 0;
    bool checkboxValue = false;
    int radioValue = 0;
    
    // Controles de entrada
    std::string textInput = "FluentGUI";
    float sliderFloat = 0.5f;
    int sliderInt = 5;
    int comboSelection = 0;
    
    // Progress
    float progressValue = 0.0f;
    
    // Tabs
    int activeTab = 0;
    
    // ListView
    int selectedListItem = -1;
    
    // TreeView
    bool treeItem1Open = false;
    bool treeItem2Open = false;
    bool treeItem3Open = false;
    
    // Modal
    bool showModal = false;
    
    // Temas
    int themeIndex = 1; // 0=Light, 1=Dark, 2+=Custom
    int accentIndex = 0; // Índice del color de acento
    
    // ScrollView
    Vec2 scrollOffset = Vec2(0, 0);
};

// Colores de acento disponibles
const std::vector<Color> accentColors = {
    FluentColors::AccentBlue,
    FluentColors::AccentGreen,
    FluentColors::AccentPurple,
    FluentColors::AccentOrange,
    FluentColors::AccentPink,
    FluentColors::AccentTeal
};

const std::vector<std::string> accentNames = {
    "Blue", "Green", "Purple", "Orange", "Pink", "Teal"
};

void ShowWidgetsBasicTab(DemoState& state) {
    Label("Widgets Básicos", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // Buttons
    Label("Buttons", std::nullopt, TypographyStyle::Subtitle);
    BeginHorizontal(8.0f);
    if (Button("Primary Button")) {
        state.clickCount++;
        std::cout << "Button clicked! Count: " << state.clickCount << std::endl;
    }
    if (Button("Secondary", Vec2(120, 32))) {
        std::cout << "Secondary button clicked!" << std::endl;
    }
    if (Button("Disabled", Vec2(120, 32), Vec2(0, 0), false)) {
        // No debería ejecutarse
    }
    EndHorizontal();
    Tooltip("Estos son botones básicos. El botón deshabilitado no responde.", 0.3f);
    
    Spacing(5);
    Label("Clicks: " + std::to_string(state.clickCount), std::nullopt, TypographyStyle::Body);
    
    Separator();
    Spacing(5);
    
    // Labels
    Label("Labels", std::nullopt, TypographyStyle::Subtitle);
    Label("This is a Body label", std::nullopt, TypographyStyle::Body);
    Label("This is a Caption label", std::nullopt, TypographyStyle::Caption);
    Label("This is a Subtitle label", std::nullopt, TypographyStyle::Subtitle);
    Label("This is a Title label", std::nullopt, TypographyStyle::Title);
    
    Separator();
    Spacing(5);
    
    // Checkbox y RadioButtons
    Label("Checkbox & RadioButtons", std::nullopt, TypographyStyle::Subtitle);
    if (Checkbox("Enable feature", &state.checkboxValue)) {
        std::cout << "Checkbox: " << (state.checkboxValue ? "ON" : "OFF") << std::endl;
    }
    
    Spacing(5);
    Label("Select option:");
    if (RadioButton("Option 1", &state.radioValue, 0)) {
        std::cout << "Selected option 1" << std::endl;
    }
    if (RadioButton("Option 2", &state.radioValue, 1)) {
        std::cout << "Selected option 2" << std::endl;
    }
    if (RadioButton("Option 3", &state.radioValue, 2)) {
        std::cout << "Selected option 3" << std::endl;
    }
    Label("Selected: Option " + std::to_string(state.radioValue + 1));
}

void ShowInputControlsTab(DemoState& state) {
    Label("Controles de Entrada", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // TextInput
    Label("Text Input", std::nullopt, TypographyStyle::Subtitle);
    if (TextInput("Name", &state.textInput, 250.0f)) {
        std::cout << "Text changed: " << state.textInput << std::endl;
    }
    Label("You typed: " + state.textInput);
    
    Separator();
    Spacing(5);
    
    // Sliders
    Label("Sliders", std::nullopt, TypographyStyle::Subtitle);
    if (SliderFloat("Speed", &state.sliderFloat, 0.0f, 1.0f, 250.0f)) {
        std::cout << "Speed: " << state.sliderFloat << std::endl;
    }
    
    if (SliderInt("Iterations", &state.sliderInt, 1, 20, 250.0f)) {
        std::cout << "Iterations: " << state.sliderInt << std::endl;
    }
    
    Separator();
    Spacing(5);
    
    // ComboBox
    Label("ComboBox", std::nullopt, TypographyStyle::Subtitle);
    std::vector<std::string> items = {"Item 1", "Item 2", "Item 3", "Item 4", "Item 5"};
    if (ComboBox("Select Item", &state.comboSelection, items, 250.0f)) {
        std::cout << "Selected: " << items[state.comboSelection] << std::endl;
    }
    Label("Selected: " + items[state.comboSelection]);
    
    Separator();
    Spacing(5);
    
    // ProgressBar
    Label("Progress Bar", std::nullopt, TypographyStyle::Subtitle);
    state.progressValue += 0.01f;
    if (state.progressValue > 1.0f) state.progressValue = 0.0f;
    ProgressBar(state.progressValue, Vec2(250, 20), "Loading... " + std::to_string((int)(state.progressValue * 100)) + "%");
}

void ShowContainersTab(DemoState& state, UIContext* ctx) {
    Label("Contenedores", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // Panel - usar layout automático
    Label("Panel", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (BeginPanel("DemoPanel", Vec2(300, 200), true, false, 0.8f)) {
        Label("This is inside a Panel");
        Label("Panels support Acrylic effect");
        if (Button("TEST BUTTON WITH LONGER TEXT")) {
            std::cout << "Button in panel clicked!" << std::endl;
        }
    }
    EndPanel();
    
    Separator();
    Spacing(5);
    
    // ScrollView - usar layout automático
    Label("ScrollView", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (BeginScrollView("DemoScroll", Vec2(300, 150), &state.scrollOffset)) {
        for (int i = 0; i < 20; i++) {
            Label("Scrollable Item " + std::to_string(i + 1));
            if (i < 19) Spacing(5);
        }
    }
    EndScrollView();
    
    Separator();
    Spacing(5);
    
    // ListView - usar layout automático
    Label("ListView", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    std::vector<std::string> listItems = {"Item A", "Item B", "Item C", "Item D", "Item E"};
    if (BeginListView("DemoList", Vec2(300, 150), &state.selectedListItem, listItems)) {
        // ListView maneja su propio contenido
    }
    EndListView();
    
    if (state.selectedListItem >= 0) {
        Label("Selected: " + listItems[state.selectedListItem]);
    }
    
    Separator();
    Spacing(5);
    
    // TreeView - usar layout automático
    Label("TreeView", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (BeginTreeView("DemoTree", Vec2(300, 200))) {
        if (TreeNode("TreeItem1", "Node 1", &state.treeItem1Open, nullptr)) {
            TreeNodePush();
            Label("Child 1.1");
            Label("Child 1.2");
            TreeNodePop();
        }
        if (TreeNode("TreeItem2", "Node 2", &state.treeItem2Open, nullptr)) {
            TreeNodePush();
            Label("Child 2.1");
            if (TreeNode("TreeItem3", "Child 2.2", &state.treeItem3Open, nullptr)) {
                TreeNodePush();
                Label("Grandchild 2.2.1");
                TreeNodePop();
            }
            TreeNodePop();
        }
        Label("Node 3 (no children)");
    }
    EndTreeView();
}

void ShowSpecializedTab(DemoState& state) {
    Label("Widgets Especializados", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // Tooltip
    Label("Tooltip", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (Button("Hover me for tooltip")) {
        // Button click
    }
    Tooltip("Este es un tooltip que aparece después de un delay", 0.5f);
    
    Spacing(10);
    Separator();
    Spacing(10);
    
    // Context Menu
    Label("Context Menu", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    Label("Click derecho en este panel para ver el menú contextual");
    Spacing(5);
    if (BeginPanel("ContextPanel", Vec2(300, 100), true)) {
        Label("Right-click here");
    }
    EndPanel();
    
    if (BeginContextMenu("DemoContextMenu")) {
        if (ContextMenuItem("Copy")) {
            std::cout << "Copy selected" << std::endl;
        }
        if (ContextMenuItem("Paste")) {
            std::cout << "Paste selected" << std::endl;
        }
        ContextMenuSeparator();
        if (ContextMenuItem("Delete", false)) {
            std::cout << "Delete selected" << std::endl;
        }
    }
    EndContextMenu();
    
    Spacing(10);
    Separator();
    Spacing(10);
    
    // Modal
    Label("Modal Dialog", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (Button("Show Modal")) {
        state.showModal = true;
    }
    
    if (BeginModal("DemoModal", "Confirm Action", &state.showModal, Vec2(400, 200))) {
        Label("This is a modal dialog.");
        Label("Click outside or the X button to close.");
        Spacing(10);
        if (Button("OK", Vec2(100, 32))) {
            state.showModal = false;
            std::cout << "Modal OK clicked" << std::endl;
        }
        SameLine(10.0f);
        if (Button("Cancel", Vec2(100, 32))) {
            state.showModal = false;
            std::cout << "Modal Cancel clicked" << std::endl;
        }
    }
    EndModal();
}

void ShowThemesTab(DemoState& state, UIContext* ctx) {
    Label("Temas y Personalización", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // Selector de tema
    Label("Tema Base", std::nullopt, TypographyStyle::Subtitle);
    BeginHorizontal(8.0f);
    if (RadioButton("Light", &state.themeIndex, 0)) {
        ctx->style = GetDefaultFluentStyle();
    }
    if (RadioButton("Dark", &state.themeIndex, 1)) {
        ctx->style = GetDarkFluentStyle();
    }
    EndHorizontal();
    
    Separator();
    Spacing(5);
    
    // Selector de color de acento
    Label("Color de Acento", std::nullopt, TypographyStyle::Subtitle);
    std::vector<std::string> accentItems;
    for (const auto& name : accentNames) {
        accentItems.push_back(name);
    }
    
    if (ComboBox("Accent Color", &state.accentIndex, accentItems, 200.0f)) {
        // Aplicar tema personalizado
        bool isDark = state.themeIndex == 1;
        ctx->style = CreateCustomFluentStyle(accentColors[state.accentIndex], isDark);
        state.themeIndex = 2; // Marcar como custom
    }
    
    Separator();
    Spacing(5);
    
    // Información
    Label("Información del Tema", std::nullopt, TypographyStyle::Subtitle);
    Label("Tema actual: " + std::string(state.themeIndex == 0 ? "Light" : 
                                         state.themeIndex == 1 ? "Dark" : "Custom"));
    Label("Color de acento: " + accentNames[state.accentIndex]);
}

void ShowRendererTab(DemoState& state, UIContext* ctx) {
    Label("Primitivas de Renderizado", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    Label("Líneas y Formas", std::nullopt, TypographyStyle::Subtitle);
    
    Spacing(5);
    Label("Estas primitivas se dibujan directamente usando el Renderer.");
    Label("Se muestran después de los widgets UI.");
    Spacing(10);
    
    // Guardar posición actual para dibujar las primitivas
    Vec2 currentPos = ctx->cursorPos;
    Vec2 viewport = ctx->renderer.GetViewportSize();
    
    // Dibujar primitivas dentro del área del TabView
    // Líneas decorativas
    float time = ctx->time;
    float y = currentPos.y + 50.0f + std::sin(time * 2.0f) * 20.0f;
    
    // Obtener el ancho disponible del área de contenido
    float contentWidth = std::min(600.0f, viewport.x - currentPos.x - 40.0f);
    
    ctx->renderer.DrawLine(Vec2(currentPos.x, y), Vec2(currentPos.x + contentWidth, y), 
                          Color(0.3f, 0.6f, 1.0f, 1.0f), 3.0f);
    
    // Círculos
    ctx->renderer.DrawCircle(Vec2(currentPos.x + 50.0f, y + 80.0f), 35.0f, 
                            Color(0.9f, 0.3f, 0.4f, 0.9f), true);
    ctx->renderer.DrawCircle(Vec2(currentPos.x + 150.0f, y + 80.0f), 35.0f, 
                            Color(0.9f, 0.8f, 0.3f, 0.9f), false);
    
    // Avanzar el cursor para que los siguientes widgets no se superpongan
    Spacing(150);
}

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("FluentGUI Demo - Complete Showcase", 
                                          1200, 1000, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    // Crear contexto UI
    UIContext* ctx = CreateContext(window);
    if (!ctx) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // Configurar tema oscuro por defecto
    ctx->style = GetDarkFluentStyle();
    
    SDL_StartTextInput(window);

    bool running = true;
    SDL_Event e;
    DemoState state;
    
    uint64_t lastTime = SDL_GetTicks();
    
    // Tabs del demo principal
    std::vector<std::string> mainTabs = {
        "Widgets Básicos",
        "Controles",
        "Contenedores",
        "Especializados",
        "Temas",
        "Renderer"
    };
    
    while (running) {
        // Calcular deltaTime
        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        deltaTime = std::min(deltaTime, 0.1f);
        lastTime = currentTime;
        
        ctx->input.Update(window);
        
        // Procesar eventos
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                int width, height;
                SDL_GetWindowSize(window, &width, &height);
                ctx->renderer.SetViewport(width, height);
            }
            else
                ctx->input.ProcessEvent(e);
        }

        // Nuevo frame
        NewFrame(deltaTime);

        // Guardar referencia para dibujar primitivas después
        Vec2 menuBarEndPos = Vec2(0, 0);

        // Menu Bar
        if (BeginMenuBar()) {
            if (BeginMenu("File")) {
                if (MenuItem("New")) {
                    std::cout << "File > New" << std::endl;
                }
                if (MenuItem("Open")) {
                    std::cout << "File > Open" << std::endl;
                }
                MenuSeparator();
                if (MenuItem("Exit")) {
                    running = false;
                }
                EndMenu();
            }
            if (BeginMenu("Edit")) {
                if (MenuItem("Copy")) {
                    std::cout << "Edit > Copy" << std::endl;
                }
                if (MenuItem("Paste")) {
                    std::cout << "Edit > Paste" << std::endl;
                }
                EndMenu();
            }
            if (BeginMenu("Help")) {
                if (MenuItem("About")) {
                    std::cout << "Help > About" << std::endl;
                }
                EndMenu();
            }
            EndMenuBar();
            menuBarEndPos = ctx->cursorPos;
        }
        
        // Contenido principal con tabs
        BeginVertical();
        
        Label("FluentGUI - Demo Completo", std::nullopt, TypographyStyle::Title);
        Separator();
        
        if (BeginTabView("MainTabs", &state.activeTab, mainTabs, Vec2(0, 900))) {
            switch (state.activeTab) {
                case 0: ShowWidgetsBasicTab(state); break;
                case 1: ShowInputControlsTab(state); break;
                case 2: ShowContainersTab(state, ctx); break;
                case 3: ShowSpecializedTab(state); break;
                case 4: ShowThemesTab(state, ctx); break;
                case 5: ShowRendererTab(state, ctx); break;
            }
        }
        EndTabView();
        
        EndVertical();
        
        // Render deferred dropdowns (ComboBox) on top of everything
        RenderDeferredDropdowns();
        
        // Renderizar
        Render();

        SDL_GL_SwapWindow(window);
    }

    DestroyContext();
    SDL_StopTextInput(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
