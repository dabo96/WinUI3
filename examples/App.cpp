#include "App.h"
#include <SDL3/SDL_vulkan.h>
#include <cmath>
#include <cstdio>

using namespace FluentUI;

App::App(const char *titulo)
    : window(nullptr), ctx(nullptr), m_textInput(""), m_searchText(""), m_multilineText(""),
      m_passwordText(""), m_modalInput(""), m_statusText("Ready") {
    
    std::cout << "Initializing SDL..." << std::endl;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return;
    }

    // Pick the window flag that matches the configured render backend.
    m_useVulkan = (GetPreferredBackend() == RenderBackendType::Vulkan);

    Uint32 apiFlag = SDL_WINDOW_OPENGL;
    if (m_useVulkan) {
        // If SDL itself has Vulkan support, let it own the surface (needs the
        // SDL_WINDOW_VULKAN flag). If not, create a plain window — the backend
        // builds a native (Win32) surface from the HWND instead.
        if (SDL_Vulkan_LoadLibrary(nullptr)) {
            apiFlag = SDL_WINDOW_VULKAN;
        } else {
            apiFlag = 0;
            std::cout << "SDL has no Vulkan support (" << SDL_GetError()
                      << "); backend will create a native surface." << std::endl;
        }
    }

    std::cout << "Creating Window (" << (m_useVulkan ? "Vulkan" : "OpenGL") << ")..." << std::endl;
    Uint32 winFlags = SDL_WINDOW_RESIZABLE | apiFlag;
    window = SDL_CreateWindow(titulo, 1280, 720, winFlags);

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    std::cout << "Creating FluentUI Context..." << std::endl;
    ctx = CreateContext(window);
    if (!ctx) {
        std::cerr << "Failed to create FluentUI Context" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        window = nullptr;
        return;
    }

    ctx->style = GetDarkFluentStyle();
    
    // IMPORTANTE: Sincronizar viewport inicial
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    ctx->renderer.SetViewport(w, h);
    
    SDL_StartTextInput(window);
    lastTime = SDL_GetTicks();
    
    std::cout << "Initializing App State..." << std::endl;

    // Initialize tab labels
    m_mainTabLabels = {"Basic", "Input", "Containers", "Lists & Trees",
                       "Overlays", "Theme",
                       "Controls", "Feedback", "Collections", "Layout",
                       "Navigation"};

    // Editable DataGrid demo data (brief 16): 6 rows x 4 logical columns.
    m_gridRows = {
        {"Alice",   "Engineering", "98",  "true"},
        {"Bob",     "Design",      "87",  "false"},
        {"Carol",   "Marketing",   "75",  "true"},
        {"Dave",    "Sales",       "91",  "false"},
        {"Erin",    "Engineering", "82",  "true"},
        {"Frank",   "Support",     "69",  "false"},
    };

    // NavFrame starts on the "home" page (brief 13).
    NavigateTo(m_navFrame, "home");

    m_containerTabLabels = {"Panel", "ScrollView", "Nested"};

    // ComboBox items
    m_comboItems = {"Option A", "Option B", "Option C", "Option D", "Option E"};
    m_fontItems = {"Segoe UI", "Arial", "Consolas", "Calibri", "Verdana"};

    // ListView items
    m_listItems = {"Dashboard",    "Messages",  "Calendar",
                   "Contacts",     "Settings",  "Notifications",
                   "Tasks",        "Documents", "Photos",
                   "Music",        "Videos",    "Downloads"};

    m_fileListItems = {"main.cpp",      "App.h",       "App.cpp",
                       "Renderer.cpp",  "Context.cpp", "Widgets.h",
                       "FluentTheme.h", "Style.h",     "Layout.h",
                       "CMakeLists.txt"};

    // Accent color names
    m_accentNames = {"Blue", "Green", "Purple", "Orange", "Pink", "Teal"};
}

App::~App() {
    DestroyContext();
    SDL_StopTextInput(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void App::Run() {
    if (!window || !ctx) {
        std::cerr << "Cannot run App: Initialization failed." << std::endl;
        return;
    }

    SDL_Event e;
    bool running = true;
    std::cout << "Starting main loop..." << std::endl;

    while (running) {
        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        deltaTime = std::min(deltaTime, 0.1f);
        lastTime = currentTime;

        ctx->input.Update(window);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                int width, height;
                SDL_GetWindowSize(window, &width, &height);
                ctx->renderer.SetViewport(width, height);
            } else
                ctx->input.ProcessEvent(e);
        }

        // Animate progress bar
        if (m_progressAnimating) {
            m_progressValue += deltaTime * 0.15f;
            if (m_progressValue > 1.0f)
                m_progressValue = 0.0f;
        }

        NewFrame(deltaTime);
        BuildUI();
        // brief 15: flush the toast queue once per frame, on the overlay layer,
        // just before rendering.
        RenderToasts(ctx);
        Render();
        // OpenGL presents via the swap; the Vulkan backend presents inside EndFrame().
        if (!m_useVulkan)
            SDL_GL_SwapWindow(window);
    }
}

void App::BuildUI() {
    BuildMenuBar();

    Vec2 viewport = ctx->renderer.GetViewportSize();

    // Main content area below menu bar
    float menuBarH = 32.0f;
    Vec2 contentPos(ctx->style.padding, menuBarH + ctx->style.padding);
    Vec2 contentSize(viewport.x - ctx->style.padding * 2.0f,
                     viewport.y - menuBarH - ctx->style.padding * 2.0f);

    ctx->cursorPos = contentPos;

    // Title
    Label("FluentGUI Widget Gallery", std::nullopt, TypographyStyle::Title);
    Spacing(2);
    Label(m_statusText, std::nullopt, TypographyStyle::Caption, true);
    Spacing(8);

    // Main tab view
    float tabHeight = contentSize.y - 70.0f;
    if (tabHeight < 200.0f)
        tabHeight = 200.0f;

    if (BeginTabView("main_tabs", &m_mainTab, m_mainTabLabels,
                     Vec2(contentSize.x, tabHeight))) {
        switch (m_mainTab) {
        case 0:
            BuildBasicWidgets();
            break;
        case 1:
            BuildInputWidgets();
            break;
        case 2:
            BuildContainers();
            break;
        case 3:
            BuildListsAndTrees();
            break;
        case 4:
            BuildOverlays();
            break;
        case 5:
            BuildThemeSettings();
            break;
        case 6:
            BuildControls();
            break;
        case 7:
            BuildFeedback();
            break;
        case 8:
            BuildCollections();
            break;
        case 9:
            BuildLayout();
            break;
        case 10:
            BuildNavigation();
            break;
        }
        EndTabView();
    }

    // Render deferred overlays (dropdowns, context menus)
    RenderDeferredDropdowns();
}

// --- Menu Bar ---------------------------------------------------------------

void App::BuildMenuBar() {
    if (BeginMenuBar()) {
        if (BeginMenu("File")) {
            if (MenuItem("New"))
                m_statusText = "File > New clicked";
            if (MenuItem("Open"))
                m_statusText = "File > Open clicked";
            if (MenuItem("Save"))
                m_statusText = "File > Save clicked";
            MenuSeparator();
            if (MenuItem("Exit"))
                m_statusText = "File > Exit clicked";
            EndMenu();
        }
        if (BeginMenu("Edit")) {
            if (MenuItem("Undo"))
                m_statusText = "Edit > Undo clicked";
            if (MenuItem("Redo"))
                m_statusText = "Edit > Redo clicked";
            MenuSeparator();
            if (MenuItem("Cut"))
                m_statusText = "Edit > Cut clicked";
            if (MenuItem("Copy"))
                m_statusText = "Edit > Copy clicked";
            if (MenuItem("Paste"))
                m_statusText = "Edit > Paste clicked";
            EndMenu();
        }
        if (BeginMenu("View")) {
            if (MenuItem("Toggle Theme")) {
                m_isDarkTheme = !m_isDarkTheme;
                ctx->style =
                    m_isDarkTheme ? GetDarkFluentStyle() : GetDefaultFluentStyle();
                m_statusText =
                    m_isDarkTheme ? "Switched to Dark Theme" : "Switched to Light Theme";
            }
            MenuSeparator();
            if (MenuItem("Zoom In"))
                m_statusText = "View > Zoom In";
            if (MenuItem("Zoom Out"))
                m_statusText = "View > Zoom Out";
            EndMenu();
        }
        if (BeginMenu("Help")) {
            if (MenuItem("About"))
                m_modalOpen = true;
            if (MenuItem("Documentation"))
                m_statusText = "Help > Documentation";
            EndMenu();
        }
        EndMenuBar();
    }
}

// --- Tab 0: Basic Widgets ---------------------------------------------------

void App::BuildBasicWidgets() {
    // --- Buttons section ---
    Label("Buttons", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);

    BeginHorizontal(8.0f);
    if (Button("Primary Button")) {
        m_buttonClickCount++;
        m_statusText =
            "Button clicked " + std::to_string(m_buttonClickCount) + " times";
    }
    if (Button("Secondary")) {
        m_statusText = "Secondary button clicked";
    }
    Button("Disabled", Vec2(0, 0), std::nullopt, false);
    EndHorizontal();

    Spacing(4);
    char clickBuf[64];
    snprintf(clickBuf, sizeof(clickBuf), "Click count: %d", m_buttonClickCount);
    Label(clickBuf, std::nullopt, TypographyStyle::Caption);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Labels section ---
    Label("Typography Variants", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    Label("Title Large", std::nullopt, TypographyStyle::TitleLarge);
    Label("Title", std::nullopt, TypographyStyle::Title);
    Label("Subtitle", std::nullopt, TypographyStyle::Subtitle);
    Label("Body (default)", std::nullopt, TypographyStyle::Body);
    Label("Body Strong", std::nullopt, TypographyStyle::BodyStrong);
    Label("Caption text", std::nullopt, TypographyStyle::Caption);
    Label("Disabled label", std::nullopt, TypographyStyle::Body, true);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Checkboxes ---
    Label("Checkboxes", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    if (Checkbox("Enable notifications", &m_checkbox1)) {
        m_statusText = m_checkbox1 ? "Notifications enabled" : "Notifications disabled";
    }
    if (Checkbox("Dark mode (synced)", &m_isDarkTheme)) {
        // Preservar el acento elegido al cambiar de tema (no resetear a azul).
        Color accents[] = {FluentColors::AccentBlue,  FluentColors::AccentGreen,
                           FluentColors::AccentPurple, FluentColors::AccentOrange,
                           FluentColors::AccentPink,   FluentColors::AccentTeal};
        ctx->style = CreateCustomFluentStyle(accents[m_accentColorIdx], m_isDarkTheme);
        m_statusText =
            m_isDarkTheme ? "Dark mode ON" : "Dark mode OFF";
    }
    Checkbox("Remember me", &m_checkbox3);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Radio Buttons ---
    Label("Radio Buttons", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    RadioButton("Small", &m_radioSelection, 0, "size");
    RadioButton("Medium", &m_radioSelection, 1, "size");
    RadioButton("Large", &m_radioSelection, 2, "size");
    RadioButton("Extra Large", &m_radioSelection, 3, "size");

    Spacing(4);
    const char *sizeNames[] = {"Small", "Medium", "Large", "Extra Large"};
    std::string radioStatus = std::string("Selected size: ") + sizeNames[m_radioSelection];
    Label(radioStatus, std::nullopt, TypographyStyle::Caption);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Progress Bars ---
    Label("Progress Bars", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);

    char progBuf[32];
    snprintf(progBuf, sizeof(progBuf), "%.0f%%", m_progressValue * 100.0f);
    ProgressBar(m_progressValue, Vec2(400, 20), progBuf);
    Spacing(2);
    ProgressBar(0.75f, Vec2(400, 8));
    Spacing(2);
    ProgressBar(0.33f, Vec2(400, 14), "Loading...");

    Spacing(4);
    if (Checkbox("Animate progress", &m_progressAnimating)) {
        m_statusText =
            m_progressAnimating ? "Progress animating" : "Progress paused";
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Separators ---
    Label("Separators", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    Label("Content above separator");
    Separator();
    Label("Content below separator");
}

// --- Tab 1: Input Widgets ---------------------------------------------------

void App::BuildInputWidgets() {
    // --- Sliders ---
    Label("Sliders", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    SliderFloat("Opacity", &m_sliderFloat, 0.0f, 1.0f, 300.0f, "%.2f");
    SliderInt("Quantity", &m_sliderInt, 0, 100, 300.0f);
    SliderFloat("Volume", &m_sliderVolume, 0.0f, 100.0f, 300.0f, "%.0f");

    Spacing(4);
    char sliderBuf[128];
    snprintf(sliderBuf, sizeof(sliderBuf),
             "Opacity: %.2f  |  Quantity: %d  |  Volume: %.0f",
             m_sliderFloat, m_sliderInt, m_sliderVolume);
    Label(sliderBuf, std::nullopt, TypographyStyle::Caption);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Text Input ---
    Label("Text Input", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    TextInput("Username", &m_textInput, 300.0f);
    Spacing(2);
    TextInput("Search", &m_searchText, 300.0f);
    Spacing(2);
    TextInput("Password", &m_passwordText, 300.0f);

    Spacing(4);
    if (!m_textInput.empty()) {
        std::string inputStatus = "Input: " + m_textInput;
        Label(inputStatus, std::nullopt, TypographyStyle::Caption);
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Multiline Text ---
    Label("Multiline Text Input", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    TextInput("Notes", &m_multilineText, 400.0f, true);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- ComboBox ---
    Label("ComboBox / Dropdown", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    ComboBox("Select option", &m_comboSelection, m_comboItems, 250.0f);
    Spacing(2);
    ComboBox("Font family", &m_comboFont, m_fontItems, 250.0f);

    Spacing(4);
    std::string comboStatus =
        "Selected: " + m_comboItems[m_comboSelection] +
        "  |  Font: " + m_fontItems[m_comboFont];
    Label(comboStatus, std::nullopt, TypographyStyle::Caption);
}

// --- Tab 2: Containers ------------------------------------------------------

void App::BuildContainers() {
    if (BeginTabView("container_tabs", &m_containerTab, m_containerTabLabels,
                     Vec2(0, 400))) {
        switch (m_containerTab) {
        case 0: {
            // --- Panels ---
            Label("Panels", std::nullopt, TypographyStyle::Subtitle);
            Spacing(4);

            if (BeginPanel("demo_panel_1", Vec2(400, 120))) {
                Label("Standard Panel", std::nullopt, TypographyStyle::BodyStrong);
                Spacing(2);
                Label("This is a standard panel with default styling.");
                Spacing(2);
                if (Button("Panel Button")) {
                    m_statusText = "Panel button clicked";
                }
                EndPanel();
            }

            Spacing(8);

            if (BeginPanel("demo_panel_acrylic", Vec2(400, 100), true, true, 0.7f)) {
                Label("Acrylic Panel", std::nullopt, TypographyStyle::BodyStrong);
                Spacing(2);
                Label("Panel with acrylic blur effect enabled.");
                EndPanel();
            }

            Spacing(8);

            if (BeginPanel("demo_panel_3", Vec2(400, 80))) {
                Label("Compact Panel", std::nullopt, TypographyStyle::BodyStrong);
                Label("Panels auto-size to content.");
                EndPanel();
            }
            break;
        }
        case 1: {
            // --- ScrollView ---
            Label("ScrollView", std::nullopt, TypographyStyle::Subtitle);
            Spacing(4);
            Label("Scroll down to see more content:", std::nullopt,
                  TypographyStyle::Caption);
            Spacing(4);

            if (BeginScrollView("demo_scroll", Vec2(450, 250), &m_scrollOffset)) {
                for (int i = 0; i < 30; i++) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Scrollable item #%d", i + 1);
                    Label(buf);
                    if (i < 29)
                        Spacing(2);
                }
                EndScrollView();
            }

            Spacing(4);
            char scrollBuf[64];
            snprintf(scrollBuf, sizeof(scrollBuf), "Scroll Y: %.1f",
                     m_scrollOffset.y);
            Label(scrollBuf, std::nullopt, TypographyStyle::Caption);
            break;
        }
        case 2: {
            // --- Nested containers ---
            Label("Nested Containers", std::nullopt, TypographyStyle::Subtitle);
            Spacing(4);

            if (BeginPanel("outer_panel", Vec2(500, 280))) {
                Label("Outer Panel", std::nullopt, TypographyStyle::BodyStrong);
                Spacing(4);

                BeginHorizontal(8.0f);
                if (BeginPanel("inner_panel_1", Vec2(220, 180))) {
                    Label("Inner Panel 1", std::nullopt, TypographyStyle::Caption);
                    Spacing(2);
                    Checkbox("Option A", &m_checkbox1);
                    Checkbox("Option B", &m_checkbox3);
                    Spacing(2);
                    if (Button("Action", Vec2(100, 0))) {
                        m_statusText = "Inner panel 1 action";
                    }
                    EndPanel();
                }
                if (BeginPanel("inner_panel_2", Vec2(220, 180))) {
                    Label("Inner Panel 2", std::nullopt, TypographyStyle::Caption);
                    Spacing(2);
                    SliderFloat("Value", &m_sliderFloat, 0.0f, 1.0f, 180.0f);
                    Spacing(2);
                    ProgressBar(m_sliderFloat, Vec2(180, 12));
                    EndPanel();
                }
                EndHorizontal();

                EndPanel();
            }
            break;
        }
        }
        EndTabView();
    }
}

// --- Tab 3: Lists & Trees ---------------------------------------------------

void App::BuildListsAndTrees() {
    Label("ListView & TreeView", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);

    BeginHorizontal(12.0f);

    // ListView 1 - Navigation
    BeginVertical(4.0f);
    Label("Navigation", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(2);
    BeginListView("nav_list", Vec2(200, 300), &m_listSelection, m_listItems);
    EndListView();
    Spacing(2);
    std::string listStatus = "Selected: " + m_listItems[m_listSelection];
    Label(listStatus, std::nullopt, TypographyStyle::Caption);
    EndVertical();

    // ListView 2 - Files
    BeginVertical(4.0f);
    Label("Files", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(2);
    BeginListView("file_list", Vec2(200, 300), &m_fileListSelection,
                  m_fileListItems);
    EndListView();
    Spacing(2);
    if (m_fileListSelection >= 0 &&
        m_fileListSelection < (int)m_fileListItems.size()) {
        std::string fileStatus = "File: " + m_fileListItems[m_fileListSelection];
        Label(fileStatus, std::nullopt, TypographyStyle::Caption);
    }
    EndVertical();

    // TreeView
    BeginVertical(4.0f);
    Label("Project Explorer", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(2);
    if (BeginTreeView("project_tree", Vec2(250, 300))) {
        if (TreeNode("src", "src/", &m_treeRoot1Open)) {
            TreeNodePush();
            if (TreeNode("core", "Core/", &m_treeSub1Open)) {
                TreeNodePush();
                TreeNode("renderer", "Renderer.cpp", nullptr, &m_treeSelected1);
                TreeNode("context", "Context.cpp", nullptr, &m_treeSelected2);
                TreeNodePop();
            }
            if (TreeNode("ui", "UI/", &m_treeSub2Open)) {
                TreeNodePush();
                TreeNode("widgets", "Widgets.cpp", nullptr, &m_treeSelected3);
                TreeNode("layout", "Layout.cpp", nullptr, &m_treeSelected4);
                TreeNodePop();
            }
            TreeNodePop();
        }
        if (TreeNode("include", "include/", &m_treeRoot2Open)) {
            TreeNodePush();
            TreeNode("fluentgui_h", "FluentGUI.h", nullptr, &m_treeSelected5);
            TreeNodePop();
        }
        if (TreeNode("examples", "examples/", &m_treeRoot3Open)) {
            TreeNodePush();
            TreeNode("app_h", "App.h");
            TreeNode("app_cpp", "App.cpp");
            TreeNodePop();
        }
        EndTreeView();
    }
    EndVertical();

    EndHorizontal();
}

// --- Tab 4: Overlays --------------------------------------------------------

void App::BuildOverlays() {
    // --- Modals ---
    Label("Modal Dialogs", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);

    BeginHorizontal(8.0f);
    if (Button("Open About Modal")) {
        m_modalOpen = true;
    }
    if (Button("Open Confirm Modal")) {
        m_confirmModalOpen = true;
    }
    EndHorizontal();

    // About modal
    if (BeginModal("about_modal", "About FluentGUI", &m_modalOpen,
                   Vec2(420, 250))) {
        Label("FluentGUI v1.0", std::nullopt, TypographyStyle::Title);
        Spacing(4);
        Label("An ImGui-style UI library with Fluent Design styling.");
        Spacing(2);
        Label("Built with C++20, OpenGL 4.5, SDL3, FreeType.",
              std::nullopt, TypographyStyle::Caption);
        Spacing(8);
        TextInput("Feedback", &m_modalInput, 350.0f);
        Spacing(8);
        if (Button("Close", Vec2(100, 0))) {
            m_modalOpen = false;
            if (!m_modalInput.empty()) {
                m_statusText = "Feedback: " + m_modalInput;
            }
        }
        EndModal();
    }

    // Confirm modal
    if (BeginModal("confirm_modal", "Confirm Action", &m_confirmModalOpen,
                   Vec2(350, 180))) {
        Label("Are you sure you want to proceed?");
        Label("This action cannot be undone.", std::nullopt,
              TypographyStyle::Caption, true);
        Spacing(12);
        BeginHorizontal(8.0f);
        if (Button("Yes, Proceed")) {
            m_confirmModalOpen = false;
            m_statusText = "Action confirmed";
        }
        if (Button("Cancel")) {
            m_confirmModalOpen = false;
            m_statusText = "Action cancelled";
        }
        EndHorizontal();
        EndModal();
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Tooltips ---
    Label("Tooltips", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    Label("Hover over the buttons below to see tooltips:");
    Spacing(4);

    BeginHorizontal(8.0f);
    Button("Save File");
    Tooltip("Save the current file to disk (Ctrl+S)", 0.3f);

    Button("Delete");
    Tooltip("Permanently delete the selected item", 0.3f);

    Button("Settings");
    Tooltip("Open application settings", 0.3f);

    Button("Info");
    Tooltip("Shows detailed information about the selected item.", 0.5f);
    EndHorizontal();

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Context Menu ---
    Label("Context Menu", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    Label("Right-click anywhere to open the context menu.");
    Spacing(4);

    if (BeginPanel("ctx_menu_area", Vec2(400, 120))) {
        Label("Right-click inside this panel", std::nullopt,
              TypographyStyle::Caption, true);
        Spacing(4);
        Label("Context menus appear at the mouse position.");

        if (BeginContextMenu("demo_context")) {
            if (ContextMenuItem("Cut"))
                m_statusText = "Context: Cut";
            if (ContextMenuItem("Copy"))
                m_statusText = "Context: Copy";
            if (ContextMenuItem("Paste"))
                m_statusText = "Context: Paste";
            ContextMenuSeparator();
            if (ContextMenuItem("Select All"))
                m_statusText = "Context: Select All";
            ContextMenuItem("Disabled Item", false);
            EndContextMenu();
        }

        EndPanel();
    }
}

// --- Tab 5: Theme -----------------------------------------------------------

void App::BuildThemeSettings() {
    Label("Theme Settings", std::nullopt, TypographyStyle::Subtitle);
    Spacing(8);

    // Theme toggle
    Label("Base Theme", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(4);
    BeginHorizontal(8.0f);
    if (Button(m_isDarkTheme ? "Switch to Light" : "Switch to Dark")) {
        m_isDarkTheme = !m_isDarkTheme;
        // Reapply with current accent
        Color accents[] = {FluentColors::AccentBlue,  FluentColors::AccentGreen,
                           FluentColors::AccentPurple, FluentColors::AccentOrange,
                           FluentColors::AccentPink,   FluentColors::AccentTeal};
        ctx->style = CreateCustomFluentStyle(accents[m_accentColorIdx], m_isDarkTheme);
        m_statusText =
            m_isDarkTheme ? "Dark theme applied" : "Light theme applied";
    }
    Label(m_isDarkTheme ? "Current: Dark" : "Current: Light", std::nullopt,
          TypographyStyle::Caption);
    EndHorizontal();

    Spacing(12);
    Separator();
    Spacing(8);

    // Accent color selection
    Label("Accent Color", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(4);

    Color accents[] = {FluentColors::AccentBlue,  FluentColors::AccentGreen,
                       FluentColors::AccentPurple, FluentColors::AccentOrange,
                       FluentColors::AccentPink,   FluentColors::AccentTeal};

    for (int i = 0; i < 6; i++) {
        if (RadioButton(m_accentNames[i], &m_accentColorIdx, i, "accent")) {
            // Aplicar en el acto al elegir el color (no esperar a "Apply").
            ctx->style = CreateCustomFluentStyle(accents[m_accentColorIdx], m_isDarkTheme);
            m_statusText = "Accent: " + m_accentNames[i];
        }
    }

    Spacing(4);
    if (Button("Apply Accent Color")) {
        ctx->style = CreateCustomFluentStyle(accents[m_accentColorIdx], m_isDarkTheme);
        m_statusText = "Accent: " + m_accentNames[m_accentColorIdx];
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // Preview widgets with current theme
    Label("Theme Preview", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(4);

    if (BeginPanel("theme_preview", Vec2(500, 200))) {
        Label("Preview Panel", std::nullopt, TypographyStyle::Subtitle);
        Spacing(4);

        BeginHorizontal(8.0f);
        Button("Normal");
        Button("Hover me");
        Button("Disabled", Vec2(0, 0), std::nullopt, false);
        EndHorizontal();

        Spacing(4);
        ProgressBar(0.6f, Vec2(400, 16), "60%");
        Spacing(4);

        static bool previewCheck = true;
        static float previewSlider = 0.5f;
        Checkbox("Preview checkbox", &previewCheck);
        SliderFloat("Preview slider", &previewSlider, 0.0f, 1.0f, 300.0f);

        EndPanel();
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // Layout demo
    Label("Layout Demo", std::nullopt, TypographyStyle::BodyStrong);
    Spacing(4);

    BeginHorizontal(4.0f);
    Button("A");
    Button("B");
    Button("C");
    Button("D");
    Button("E");
    EndHorizontal();

    Spacing(4);

    BeginHorizontal(16.0f);
    Button("Wide spacing");
    Button("Between");
    Button("Buttons");
    EndHorizontal();
}

// --- Tab 6: Signature Controls (brief 14) -----------------------------------

void App::BuildControls() {
    // --- ToggleSwitch ---
    Label("ToggleSwitch", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    if (ToggleSwitch("Wi-Fi", &m_toggleWifi, "On", "Off")) {
        m_statusText = m_toggleWifi ? "Wi-Fi enabled" : "Wi-Fi disabled";
    }
    if (ToggleSwitch("Bluetooth", &m_toggleBluetooth, "On", "Off")) {
        m_statusText = m_toggleBluetooth ? "Bluetooth enabled" : "Bluetooth disabled";
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Expander ---
    Label("Expander", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    if (BeginExpander("demo_expander", "Advanced settings", Icons::Settings,
                      &m_expanderOpen)) {
        Label("These options are hidden until the card is expanded.");
        Spacing(2);
        Checkbox("Enable telemetry", &m_checkbox1);
        SliderInt("Cache size (MB)", &m_sliderInt, 0, 100, 240.0f);
        EndExpander();
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- SplitButton / DropDownButton ---
    Label("SplitButton & DropDownButton", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);

    std::vector<CommandItem> saveMenu = {
        {"Save", Icons::Save, [this]() { m_statusText = "Save"; }, true, true},
        {"Save As...", Icons::Copy, [this]() { m_statusText = "Save As"; }, false, true},
        {"Save All", Icons::FileText, [this]() { m_statusText = "Save All"; }, false, true},
    };
    BeginHorizontal(12.0f);
    SplitButton("Save", Icons::Save, [this]() { m_statusText = "Primary Save"; }, saveMenu);

    std::vector<CommandItem> exportMenu = {
        {"Export PNG", Icons::Image, [this]() { m_statusText = "Export PNG"; }, true, true},
        {"Export PDF", Icons::FileText, [this]() { m_statusText = "Export PDF"; }, true, true},
        {"Export SVG", Icons::FileText, [this]() { m_statusText = "Export SVG"; }, true, true},
    };
    DropDownButton("Export", Icons::Download, exportMenu);
    EndHorizontal();

    Spacing(12);
    Separator();
    Spacing(8);

    // --- NumberBox ---
    Label("NumberBox", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    if (NumberBox("Temperature (C)", &m_numberBoxValue, -50.0, 150.0, 1.0, "%.0f")) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Temperature set to %.0f", m_numberBoxValue);
        m_statusText = buf;
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- RatingControl ---
    Label("RatingControl", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    if (RatingControl("demo_rating", &m_rating, 5, false)) {
        m_statusText = "Rating: " + std::to_string(m_rating) + " of 5";
    }
    Spacing(2);
    Label("Half-stars:", std::nullopt, TypographyStyle::Caption);
    if (RatingControl("demo_rating_half", &m_ratingHalf, 5, true)) {
        m_statusText = "Half rating units: " + std::to_string(m_ratingHalf);
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Flyout / MenuFlyout ---
    Label("Flyout & MenuFlyout", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginHorizontal(12.0f);
    if (Button("Open Flyout", Icons::Info)) {
        OpenFlyout("demo_flyout");
    }
    Rect flyoutAnchor(ctx->lastItemPos, ctx->lastItemSize);

    if (Button("Menu Flyout", Icons::LayoutGrid)) {
        OpenFlyout("demo_menuflyout");
    }
    Rect menuAnchor(ctx->lastItemPos, ctx->lastItemSize);
    EndHorizontal();

    // Generic anchored flyout with arbitrary content.
    if (BeginFlyout("demo_flyout", flyoutAnchor, FlyoutPlacement::Bottom)) {
        Label("Quick actions", std::nullopt, TypographyStyle::BodyStrong);
        Spacing(4);
        if (Button("Refresh", Vec2(160, 0))) {
            m_statusText = "Flyout: Refresh";
            CloseFlyout("demo_flyout");
        }
        if (Button("Reset", Vec2(160, 0))) {
            m_statusText = "Flyout: Reset";
            CloseFlyout("demo_flyout");
        }
        EndFlyout();
    }

    // Menu-style flyout (icon/label/accelerator rows).
    std::vector<MenuEntry> menuEntries = {
        {"Cut", Icons::Copy, "Ctrl+X", false, false, false, true, {}, [this]() { m_statusText = "Menu: Cut"; }},
        {"Copy", Icons::Copy, "Ctrl+C", false, false, false, true, {}, [this]() { m_statusText = "Menu: Copy"; }},
        {"Paste", Icons::Copy, "Ctrl+V", false, false, false, true, {}, [this]() { m_statusText = "Menu: Paste"; }},
        {"", 0, "", false, false, true, true, {}, nullptr}, // separator
        {"Word wrap", 0, "", true, m_checkbox1, false, true, {}, [this]() { m_checkbox1 = !m_checkbox1; }},
    };
    MenuFlyout("demo_menuflyout", menuAnchor, menuEntries);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- ContentDialog ---
    Label("ContentDialog", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    if (Button("Open Content Dialog")) {
        m_contentDialogOpen = true;
    }
    if (m_contentDialogOpen) {
        DialogResult r = ContentDialog(
            "demo_content_dialog", &m_contentDialogOpen, "Rename item",
            [this]() {
                Label("Enter a new name for the selected item:");
                Spacing(6);
                TextInput("Name", &m_dialogName, 320.0f);
            },
            "Rename", "", "Cancel");
        if (r == DialogResult::Primary) {
            m_statusText = "Renamed to: " + m_dialogName;
        } else if (r == DialogResult::Close) {
            m_statusText = "Rename cancelled";
        }
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- TeachingTip ---
    Label("TeachingTip", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    Button("Feature button", Icons::Lightbulb);
    Rect tipTarget(ctx->lastItemPos, ctx->lastItemSize);
    if (TeachingTip("demo_teaching_tip", tipTarget, "New feature!",
                    "This button now does something amazing. Try it out.",
                    "Got it")) {
        m_statusText = "TeachingTip acknowledged";
    }

    // TODO brief 17/18: SelectableText, HyperlinkButton, AutoSuggestBox,
    // TokenizingTextBox, PasswordBox, MarkdownView not integrated yet.
}

// --- Tab 7: Feedback & Status (brief 15) ------------------------------------

void App::BuildFeedback() {
    // --- InfoBar (4 severities) ---
    Label("InfoBar", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    InfoBar("ib_info", InfoSeverity::Informational, "Heads up",
            "This is an informational message with extra context.", false);
    Spacing(4);
    InfoBar("ib_success", InfoSeverity::Success, "Saved",
            "Your changes were saved successfully.", true);
    Spacing(4);
    InfoBar("ib_warning", InfoSeverity::Warning, "Low disk space",
            "You are running low on storage. Consider freeing some space.",
            true, "Manage");
    Spacing(4);
    InfoBar("ib_error", InfoSeverity::Error, "Upload failed",
            "The file could not be uploaded. Check your connection and retry.",
            true, "Retry");

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Toast ---
    Label("Toast notifications", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginHorizontal(8.0f);
    if (Button("Info toast")) {
        ToastOptions opts;
        opts.severity = InfoSeverity::Informational;
        opts.durationSec = 4.0f;
        ShowToast("Notification", "An informational toast (#" +
                  std::to_string(++m_toastCounter) + ")", opts);
    }
    if (Button("Success toast")) {
        ToastOptions opts;
        opts.severity = InfoSeverity::Success;
        ShowToast("Done", "Operation completed successfully.", opts);
    }
    if (Button("Action toast")) {
        ToastOptions opts;
        opts.severity = InfoSeverity::Warning;
        opts.durationSec = 6.0f;
        opts.actionText = "Undo";
        opts.onAction = [this]() { m_statusText = "Toast action: Undo"; };
        ShowToast("Item deleted", "The item was moved to trash.", opts);
    }
    EndHorizontal();
    Label("Toasts stack in the lower-right corner (RenderToasts in the loop).",
          std::nullopt, TypographyStyle::Caption, true);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- ProgressRing ---
    Label("ProgressRing", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginHorizontal(24.0f);
    BeginVertical(2.0f);
    Label("Determinate", std::nullopt, TypographyStyle::Caption);
    ProgressRing("pr_determinate", 48.0f, m_progressValue);
    EndVertical();
    BeginVertical(2.0f);
    Label("Indeterminate", std::nullopt, TypographyStyle::Caption);
    ProgressRing("pr_indeterminate", 48.0f, -1.0f);
    EndVertical();
    EndHorizontal();

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Badge ---
    Label("Badge", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginHorizontal(40.0f);
    IconLabel(Icons::Bell, 28.0f);
    Badge(7);
    IconLabel(Icons::Mail, 28.0f);
    Badge(128); // shows "99+"
    IconLabel(Icons::User, 28.0f);
    Badge(0, true); // dot only
    EndHorizontal();

    Spacing(16);
    Separator();
    Spacing(8);

    // --- Skeleton ---
    Label("Skeleton placeholders", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginHorizontal(12.0f);
    Skeleton(Vec2(64, 64), 8.0f); // avatar block
    BeginVertical(6.0f);
    SkeletonText(3, 16.0f, 0.6f);
    EndVertical();
    EndHorizontal();
}

// --- Tab 8: Collections (brief 16) ------------------------------------------

void App::BuildCollections() {
    // --- GridView ---
    Label("GridView", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    GridView("demo_gridview", 30, Vec2(110, 80), [](int index) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Tile %d", index + 1);
        if (BeginPanel(std::string("gv_tile_") + std::to_string(index), Vec2(0, 0))) {
            Label(buf, std::nullopt, TypographyStyle::Caption);
            EndPanel();
        }
    }, 8.0f, 0.0f);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- DataGrid (one editable column) ---
    Label("DataGrid", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    std::vector<DataColumn> cols;
    {
        DataColumn c0; c0.header = "Name";   c0.width = 120.0f; c0.editable = true;  c0.type = DataColumn::Type::Text;   cols.push_back(c0);
        DataColumn c1; c1.header = "Team";   c1.width = 130.0f; c1.editable = false; c1.type = DataColumn::Type::Choice; c1.choices = {"Engineering","Design","Marketing","Sales","Support"}; cols.push_back(c1);
        DataColumn c2; c2.header = "Score";  c2.width = 80.0f;  c2.editable = true;  c2.type = DataColumn::Type::Number; cols.push_back(c2);
        DataColumn c3; c3.header = "Active"; c3.width = 70.0f;  c3.editable = true;  c3.type = DataColumn::Type::Bool;   cols.push_back(c3);
    }
    DataGridResult dgr = DataGrid(
        "demo_datagrid", cols, (int)m_gridRows.size(),
        [this](int row, int col) -> std::string {
            if (row >= 0 && row < (int)m_gridRows.size() &&
                col >= 0 && col < (int)m_gridRows[row].size())
                return m_gridRows[row][col];
            return "";
        },
        [this](int row, int col, const std::string& newVal) {
            if (row >= 0 && row < (int)m_gridRows.size() &&
                col >= 0 && col < (int)m_gridRows[row].size())
                m_gridRows[row][col] = newVal;
        });
    if (dgr.editedRow >= 0) {
        m_statusText = "DataGrid edited row " + std::to_string(dgr.editedRow) +
                       " col " + std::to_string(dgr.editedCol);
    }

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Pagination ---
    Label("Pagination", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    int page = Pagination("demo_pagination", 42, &m_paginationPage);
    Label("Current page: " + std::to_string(page + 1) + " of 42",
          std::nullopt, TypographyStyle::Caption);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- ExpanderList (accordion) ---
    Label("ExpanderList (accordion)", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    static const char* faqQ[] = {"What is FluentGUI?", "Is it cross-platform?",
                                 "Which backends?", "How is licensing?"};
    static const char* faqA[] = {
        "An immediate-mode UI library with Fluent Design styling.",
        "Yes, it runs on Windows, Linux and macOS via SDL3.",
        "OpenGL and Vulkan are both supported.",
        "See the repository for license details."};
    ExpanderList("demo_expanderlist", 4,
                 [](int i) { return std::string(faqQ[i]); },
                 [](int i) { LabelWrapped(faqA[i], 480.0f); },
                 true);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- FlipView ---
    Label("FlipView / Carousel", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    int flip = FlipView("demo_flipview", 4, [](int index) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Slide %d / 4", index + 1);
        if (BeginPanel(std::string("flip_page_") + std::to_string(index), Vec2(420, 160))) {
            Label(buf, std::nullopt, TypographyStyle::Title);
            Spacing(4);
            Label("Swipe with the arrows or dots below.");
            EndPanel();
        }
    }, &m_flipIndex);
    Label("Page index: " + std::to_string(flip), std::nullopt, TypographyStyle::Caption);
}

// --- Tab 9: Layout primitives (brief 19) ------------------------------------

void App::BuildLayout() {
    // --- WrapPanel ---
    Label("WrapPanel (resize the window to reflow)", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginWrapPanel("demo_wrap", 8.0f, 8.0f);
    static const char* chips[] = {"Design", "Engineering", "Marketing", "Sales",
                                  "Support", "Finance", "Legal", "Research",
                                  "Operations", "Product", "Security", "QA"};
    for (int i = 0; i < 12; i++) {
        Button(chips[i], ButtonSize::Small);
    }
    EndWrapPanel();

    Spacing(12);
    Separator();
    Spacing(8);

    // --- UniformGrid ---
    Label("UniformGrid (4 columns)", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginUniformGrid("demo_uniform", 4, 8.0f);
    for (int i = 0; i < 8; i++) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Cell %d", i + 1);
        if (BeginPanel(std::string("ug_cell_") + std::to_string(i), Vec2(0, 60))) {
            Label(buf, std::nullopt, TypographyStyle::Caption);
            EndPanel();
        }
        if (i < 7)
            UniformGridNextCell();
    }
    EndUniformGrid();

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Breakpoint readout ---
    Label("Responsive Breakpoint", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    Breakpoint bp = CurrentBreakpoint();
    const char* bpName = "Small";
    switch (bp) {
    case Breakpoint::Small:  bpName = "Small (< 640)";   break;
    case Breakpoint::Medium: bpName = "Medium (< 1008)"; break;
    case Breakpoint::Large:  bpName = "Large (< 1366)";  break;
    case Breakpoint::XLarge: bpName = "XLarge (>= 1366)"; break;
    }
    Label(std::string("Current breakpoint: ") + bpName, std::nullopt,
          TypographyStyle::BodyStrong);

    Spacing(12);
    Separator();
    Spacing(8);

    // --- Canvas (absolute positioning) ---
    Label("Canvas (absolute positioning)", std::nullopt, TypographyStyle::Subtitle);
    Spacing(4);
    BeginCanvas("demo_canvas", Vec2(440, 200));
    CanvasChild(Vec2(20, 20), [this]() {
        if (Button("Top-left", Icons::Home))
            m_statusText = "Canvas: top-left";
    });
    CanvasChild(Vec2(280, 80), [this]() {
        if (Button("Center-right", Icons::Star))
            m_statusText = "Canvas: center-right";
    });
    CanvasChild(Vec2(120, 150), [this]() {
        Label("Free-floating label", std::nullopt, TypographyStyle::Caption);
    });
    EndCanvas();
}

// --- Tab 10: Navigation (brief 13) ------------------------------------------

void App::BuildNavigation() {
    // Sidebar navigation items with sub-items and a badge.
    std::vector<NavItem> navItems = {
        {"home",     "Home",     Icons::Home,     0, {}},
        {"mail",     "Mail",     Icons::Mail,     5, {}},
        {"docs",     "Documents", Icons::Folder,  0, {
            {"recent",  "Recent",    Icons::FileText, 0, {}},
            {"shared",  "Shared",    Icons::User,     2, {}},
        }},
        {"media",    "Media",    Icons::Image,    0, {}},
    };
    std::vector<NavItem> footerItems = {
        {"settings", "Settings", Icons::Settings, 0, {}},
    };

    BeginHorizontal(12.0f);

    // Left: NavigationView. Returns the currently selected key.
    std::string sel = NavigationView("demo_nav", navItems, &m_selectedNavKey,
                                     NavDisplayMode::Expanded, footerItems);
    // Drive the page frame from the sidebar selection.
    if (sel != m_navFrame.current && !sel.empty()) {
        NavigateTo(m_navFrame, sel);
        m_statusText = "Navigated to: " + sel;
    }

    // Right: content area with CommandBar + Breadcrumb + NavFrame history.
    BeginVertical(8.0f);

    // CommandBar with primary + secondary (overflow) commands.
    std::vector<CommandItem> primaryCmds = {
        {"New",     Icons::Plus,     [this]() { m_statusText = "Cmd: New"; },     true, true},
        {"Save",    Icons::Save,     [this]() { m_statusText = "Cmd: Save"; },    true, true},
        {"Share",   Icons::Upload,   [this]() { m_statusText = "Cmd: Share"; },   true, true},
    };
    std::vector<CommandItem> secondaryCmds = {
        {"Settings", Icons::Settings, [this]() { m_statusText = "Cmd: Settings"; }, false, true},
        {"Help",     Icons::CircleHelp, [this]() { m_statusText = "Cmd: Help"; },   false, true},
    };
    CommandBar("demo_cmdbar", primaryCmds, secondaryCmds);

    Spacing(4);

    // BreadcrumbBar reflecting the current page.
    std::vector<std::string> crumbs = {"Workspace", "Library", m_navFrame.current};
    int clicked = BreadcrumbBar("demo_breadcrumb", crumbs);
    if (clicked >= 0) {
        m_statusText = "Breadcrumb #" + std::to_string(clicked) + " clicked";
    }

    Spacing(4);

    // NavFrame back/forward history controls.
    BeginHorizontal(8.0f);
    if (Button("Back", Icons::ChevronRight)) {
        if (NavigateBack(m_navFrame)) {
            m_selectedNavKey = m_navFrame.current;
            m_statusText = "Back to: " + m_navFrame.current;
        }
    }
    if (Button("Forward", Icons::ChevronRight)) {
        if (NavigateForward(m_navFrame)) {
            m_selectedNavKey = m_navFrame.current;
            m_statusText = "Forward to: " + m_navFrame.current;
        }
    }
    EndHorizontal();

    Spacing(8);

    // The "page" content itself.
    if (BeginPanel("nav_page_content", Vec2(0, 240))) {
        Label("Page: " + m_navFrame.current, std::nullopt, TypographyStyle::Title);
        Spacing(6);
        Label("Back stack: " + std::to_string(m_navFrame.backStack.size()) +
              "   Forward stack: " + std::to_string(m_navFrame.forwardStack.size()),
              std::nullopt, TypographyStyle::Caption);
        Spacing(6);
        Label("Select an item in the sidebar to navigate. The frame keeps a "
              "back/forward history.");
        EndPanel();
    }

    EndVertical();

    EndHorizontal();
}
