#include "App.h"
#include <cmath>
#include <cstdio>

using namespace FluentUI;

App::App(const char *titulo)
    : m_textInput(""), m_searchText(""), m_multilineText(""),
      m_passwordText(""), m_modalInput(""), m_statusText("Ready") {
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(titulo, 1280, 720,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    ctx = CreateContext(window);
    if (!ctx) {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    ctx->style = GetDarkFluentStyle();
    SDL_StartTextInput(window);
    lastTime = SDL_GetTicks();

    // Initialize tab labels
    m_mainTabLabels = {"Basic", "Input", "Containers", "Lists & Trees",
                       "Overlays", "Theme"};

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
    SDL_Event e;
    bool running = true;

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
        Render();
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
        ctx->style =
            m_isDarkTheme ? GetDarkFluentStyle() : GetDefaultFluentStyle();
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
        RadioButton(m_accentNames[i], &m_accentColorIdx, i, "accent");
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
