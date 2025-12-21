#include "App.h"

using namespace FluentUI;

App::App(const char* titulo) : m_nombreCompleto(""), m_nombreMostrado(""), m_mostrarNombre(false)
{
    // Asegurar que m_mostrarNombre esté explícitamente en false al inicio
    m_mostrarNombre = false;
    
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(titulo, 1920, 1080, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    // Inicializar FluentGUI
    ctx = CreateContext(window);

    if (!ctx) {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    //
    ctx->style = GetDarkFluentStyle();

    SDL_StartTextInput(window);

    lastTime = SDL_GetTicks();
}

App::~App()
{
    DestroyContext();
    SDL_StopTextInput(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void App::Run()
{
    SDL_Event e;
    bool running = true;


    int comboBoxIndex = 0;
    
    float progressValue = 0.0f;
    int activeTab = 0;

    std::vector<std::string> mainTabs = {
        "Widgets Básicos",
        "Controles",
        "Contenedores",
        "Widgets Avanzados",
        "Editor",
    };
    
    while (running) {

        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        deltaTime = std::min(deltaTime, 0.1f);
        lastTime = currentTime;
        
        // IMPORTANTE: Actualizar el estado de input ANTES de procesar eventos
        // Esto resetea los flags de IsMousePressed,        // Reset inputs at start of frame
        ctx->input.Update(window);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                e.window.windowID == SDL_GetWindowID(window))
                running = false;

            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                 std::cout << "[AppEvent] Mouse Down Detected: " << (int)e.button.button << std::endl;
            }

            // Procesar input
            if (ctx)
                ctx->input.ProcessEvent(e);
        }
        
        // DEBUG: Check input state IMMEDIATELY after poll loop
        if (ctx && ctx->input.IsMousePressed(0)) {
             std::cout << "[App] Post-Poll: MousePressed[0] IS TRUE for frame " << ctx->frame << std::endl;
        } else if (ctx) {
             // Optional: Log failure if we knew a click happened? No, too spammy.
        }

        // Nuevo frame
        NewFrame(deltaTime);

        // Iniciar Espacio de Docking
        BeginDockSpace("MainDockSpace");

        // Panel 1: Sidebar (Navegación)
        if (BeginPanel("Navigation", Vec2(250, 500), true, true, 0.95f, Vec2(20, 20))) {
            Label("Menu", std::nullopt, TypographyStyle::Subtitle);
            Spacing(10);
            
            static int selectedMenu = 0;
            if (Button("Dashboard", Vec2(230, 0))) selectedMenu = 0;
            if (Button("Settings", Vec2(230, 0))) selectedMenu = 1;
            if (Button("Profile", Vec2(230, 0))) selectedMenu = 2;
            
            Spacing(20);
            Separator();
            Spacing(10);
            Label("Status: Ready", std::nullopt, TypographyStyle::Caption);
        }
        EndPanel();

        // Panel 2: Contenido Principal (Tabs)
        if (BeginPanel("Main Workspace", Vec2(800, 600), true, true, 0.9f, Vec2(290, 20))) {
            if (BeginTabView("MainTabs", &activeTab, mainTabs, Vec2(0, 900))) { // Size 0,0 to fill panel
                switch (activeTab) {
                    case 0: ShowWidgetsBasicTab(); break;
                    case 1: ShowInputControlsTab(); break;
                    case 2: ShowContainersTab(ctx); break;
                    case 3: ShowAdvancedWidgetsTab(ctx); break;
                    case 4: ShowEditorTab(); break;
                }
                EndTabView();
            }
        }
        EndPanel();

        // Panel 3: Herramientas (Ejemplo extra)
         if (BeginPanel("Tools", Vec2(300, 400), true, true, 0.9f, Vec2(1110, 20))) {
             Label("Toolbox", std::nullopt, TypographyStyle::Subtitle);
             Color color;
             ColorPicker("Accent Color", &color);
             static float val = 0.5f;
             SliderFloat("Intensity", &val, 0.0f, 1.0f);
        }
        EndPanel();

        EndDockSpace();

        RenderDeferredDropdowns();
        RenderNotifications();

        // Renderizar
        Render();

        SDL_GL_SwapWindow(window);
    }
}



void App::ShowWidgetsBasicTab() {
    Label("Widgets Básicos", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // Buttons
    Label("Buttons", std::nullopt, TypographyStyle::Subtitle);
    BeginHorizontal(8.0f);
    if (Button("Primary Button")) {
        std::cout << "Button clicked!" << std::endl;
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
}

void App::ShowInputControlsTab() {
    Label("Controles de entrada", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // TextInput
    Label("TextInput", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    TextInput("TextInput", &m_nombreCompleto, 250.0f);

    Spacing(5);

    Checkbox("Enable feature", &checkboxValue);
    
    Spacing(10.0f);
    Label("Toggle Switch", std::nullopt, TypographyStyle::Subtitle);
    static bool switchState = false;
    ToggleSwitch("Dark Mode (Mock)", &switchState);

    Spacing(10.0f);
    Label("SpinBox", std::nullopt, TypographyStyle::Subtitle);
    static int spinValue = 10;
    SpinBox("Volumen", &spinValue, 0, 100);
}

void App::ShowContainersTab(FluentUI::UIContext* ctx) {
    Label("Contenedores", std::nullopt, TypographyStyle::Title);
    Spacing(5);
    
    // Panel - usar layout automático
    Label("Panel", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (BeginPanel("DemoPanel", Vec2(300, 200), true, false, 0.8f, Vec2(0, 0), false)) {
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
    if (BeginScrollView("DemoScroll", Vec2(300, 150), &scrollOffset)) {
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
    if (BeginListView("DemoList", Vec2(300, 150), &selectedListItem, listItems)) {
        // ListView maneja su propio contenido
    }
    EndListView();
    
    if (selectedListItem >= 0) {
        Label("Selected: " + listItems[selectedListItem]);
    }
    
    Separator();
    Spacing(5);
    
    // TreeView - usar layout automático
    Label("TreeView", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    if (BeginTreeView("DemoTree", Vec2(300, 200))) {
        if (TreeNode("TreeItem1", "Node 1", &treeItem1Open, nullptr)) {
            TreeNodePush();
            Label("Child 1.1");
            Label("Child 1.2");
            TreeNodePop();
        }
        if (TreeNode("TreeItem2", "Node 2", &treeItem2Open, nullptr)) {
            TreeNodePush();
            Label("Child 2.1");
            if (TreeNode("TreeItem3", "Child 2.2", &treeItem3Open, nullptr)) {
                TreeNodePush();
                Label("Grandchild 2.2.1");
                TreeNodePop();
            }
            TreeNodePop();
            TreeNodePop();
        }
        // Cambio: Usar TreeNode en lugar de Label para que sea seleccionable
        // Pasamos nullptr en el 3er argumento (isOpen) para indicar que NO tiene hijos
        static bool node3Selected = false; 
        TreeNode("TreeItem3Node", "Node 3 (no children)", nullptr, &node3Selected);
    }
    EndTreeView();
}

void App::ShowAdvancedWidgetsTab(FluentUI::UIContext* ctx) {
    Label("Widgets Avanzados", std::nullopt, TypographyStyle::Title);
    Spacing(10);
    
    // Dynamic initialization of splitter sizes
    if (splitterSize1 < 0.0f || splitterSize2 < 0.0f) {
        // Calculate available width
        // Calculate available width
        // Use layoutStack to get the actual available space in the current container (TabView)
        float availableWidth = 0.0f;
        if (!ctx->layoutStack.empty()) {
            availableWidth = ctx->layoutStack.back().availableSpace.x;
        }
        
        // Safety check if availableSpace is not set correctly or infinite
        if (availableWidth <= 0.0f || availableWidth > 10000.0f) {
             availableWidth = ctx->renderer.GetViewportSize().x;
        }
        
        // Subtract strict padding to avoid overflow/wrapping
        float effectiveWidth = availableWidth - 40.0f; 

        if (effectiveWidth > 0.0f) {
            float totalWidth = effectiveWidth;
            float splitterThickness = 4.0f;
            float padding = 0.0f; 
            float halfWidth = (totalWidth - splitterThickness) * 0.5f;
            splitterSize1 = halfWidth;
            splitterSize2 = halfWidth;
        } else {
            // Fallback
            splitterSize1 = 300.0f;
            splitterSize2 = 300.0f;
        }
    }
    
    // Splitter Demo
    Label("Splitter Demo", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    
    // Capture state for this frame to ensure visual consistency
    // Panel 1 uses 'splitterSize1' (pre-update)
    // Panel 2 needs to use 'splitterSize2' (pre-update) to avoid layout jitter
    float s2_frame = splitterSize2; 

    // Use 0 spacing to keep panels attached to the splitter
    BeginHorizontal(0.0f);
        // Panel 1
        if (BeginPanel("SplitPanel1", Vec2(splitterSize1, 200), true, true, 0.5f)) {
            Label("Left Panel");
            Label("Resize me ->");
        }
        EndPanel();
        
        // Splitter
        // Use vertical splitter for side-by-side panels
        Splitter("Split1", true, 4.0f, &splitterSize1, &splitterSize2, 150.0f, 150.0f);
        
        // Panel 2
        if (BeginPanel("SplitPanel2", Vec2(s2_frame, 200), true, true, 0.5f)) {
            Label("Right Panel");
            Label("<- Resize me");
        }
        EndPanel();
    EndHorizontal();
    
    Spacing(20);
    Separator();
    Spacing(20);
    
    // ColorPicker Demo
    Label("ColorPicker Demo", std::nullopt, TypographyStyle::Subtitle);
    Spacing(5);
    
    BeginHorizontal(20.0f);
        // Picker (now includes preview)
         ColorPicker("Color base", &colorPickerValue);
    EndHorizontal();
    
    Spacing(10);
}

void App::ShowEditorTab() {
    Label("Inspector de Objetos", std::nullopt, TypographyStyle::Title);
    Spacing(10);

    if (BeginCollapsibleGroup("Transform", true)) {
        DragVector3("Position", m_gameObjectPos, 0.1f);
        
        static float rotation[3] = {0,0,0};
        DragVector3("Rotation", rotation, 1.0f);
        
        static float scale[3] = {1,1,1};
        DragVector3("Scale", scale, 0.1f);
        EndCollapsibleGroup();
    }

    if (BeginCollapsibleGroup("Material", true)) {
        static float roughness = 0.5f;
        DragFloat("Roughness", &roughness, 0.01f, 0.0f, 1.0f);
        
        static float metallic = 0.0f;
        DragFloat("Metallic", &metallic, 0.01f, 0.0f, 1.0f);
        
        Label("Albedo Map:");
        // Placeholder texture ID 1 (font atlas usually, but serves as demo)
        Image(1, Vec2(100, 100));
        EndCollapsibleGroup();
    }

    if (BeginCollapsibleGroup("Settings", true)) {
        static int selectedResolution = 0;
        static std::vector<std::string> resolutions = {
            "1920x1080", "1600x900", "1280x720", "1024x768",
            "800x600", "640x480", "3840x2160", "2560x1440",
            "1366x768", "1280x1024", "1440x900", "1680x1050"
        };
        ComboBox("Resolution", &selectedResolution, resolutions, 200.0f);
        EndCollapsibleGroup();
    }

    if (BeginCollapsibleGroup("Notifications", true)) {
        if (Button("Success Toast")) {
            ShowNotification("File saved successfully!", ToastType::Success);
        }
        if (Button("Error Toast")) {
            ShowNotification("Compilation failed!", ToastType::Error);
        }
        if (Button("Info Toast")) {
            ShowNotification("Update available.", ToastType::Info);
        }
        if (Button("Warning Toast")) {
            ShowNotification("Disk space low.", ToastType::Warning);
        }
        EndCollapsibleGroup();
    }
}
