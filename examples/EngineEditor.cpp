#include "EngineEditor.h"
#include "core/FileDialog.h"
#include <cstdio>
#include <algorithm>

using namespace FluentUI;

// ======================== Initialization ========================

void InitEditorState(EditorState& state) {
    if (state.initialized) return;

    state.objects.push_back({"Main Camera", {0, 5, -10}, {30, 0, 0}, {1,1,1}, Color(0.3f,0.7f,1,1), true, false, 0, "MainCamera", {}, -1});
    state.objects.push_back({"Directional Light", {0, 10, 0}, {50, -30, 0}, {1,1,1}, Color(1,0.95f,0.8f,1), true, true, 0, "Light", {}, -1});
    state.objects.push_back({"Ground", {0, 0, 0}, {0, 0, 0}, {50, 1, 50}, Color(0.4f,0.5f,0.3f,1), true, true, 2, "Environment", {}, -1});
    state.objects.push_back({"Player", {0, 1, 0}, {0, 0, 0}, {1,2,1}, Color(0.2f,0.6f,1,1), true, false, 1, "Player", {4, 5}, -1});
    state.objects.push_back({"Weapon", {0.5f, 1.5f, 0.3f}, {0, 0, 0}, {0.2f,0.2f,1.0f}, Color(0.6f,0.6f,0.6f,1), true, false, 3, "Weapon", {}, 3});
    state.objects.push_back({"Shield", {-0.5f, 1.2f, 0.2f}, {0, 15, 0}, {0.8f,0.8f,0.1f}, Color(0.8f,0.7f,0.2f,1), true, false, 0, "Shield", {}, 3});
    state.objects.push_back({"Enemy_01", {5, 1, 3}, {0, -45, 0}, {1,1.5f,1}, Color(0.9f,0.2f,0.2f,1), true, false, 1, "Enemy", {}, -1});
    state.objects.push_back({"Enemy_02", {-4, 1, 7}, {0, 120, 0}, {1.2f,1.8f,1.2f}, Color(0.8f,0.15f,0.15f,1), true, false, 1, "Enemy", {}, -1});
    state.objects.push_back({"Collectible", {3, 0.5f, -2}, {0, 0, 0}, {0.5f,0.5f,0.5f}, Color(1,0.9f,0,1), true, false, 1, "Pickup", {}, -1});
    state.objects.push_back({"Wall_North", {0, 2, 15}, {0, 0, 0}, {30, 4, 1}, Color(0.5f,0.5f,0.5f,1), true, true, 0, "Environment", {}, -1});

    state.consoleMessages.push_back({ConsoleMessage::Info, "FluentUI Engine Editor initialized", 0.0f});
    state.consoleMessages.push_back({ConsoleMessage::Info, "Scene loaded: SampleScene.scene (10 objects)", 0.1f});
    state.consoleMessages.push_back({ConsoleMessage::Info, "File drag & drop enabled — drop files onto the window to import", 0.15f});
    state.consoleMessages.push_back({ConsoleMessage::Warning, "Shader 'PBR_Standard' compiled with 2 warnings", 0.2f});
    state.consoleMessages.push_back({ConsoleMessage::Info, "Physics system ready (Bullet 3.25)", 0.3f});
    state.consoleMessages.push_back({ConsoleMessage::Error, "Missing texture: 'enemy_normal.png' on Enemy_01", 0.4f});
    state.consoleMessages.push_back({ConsoleMessage::Info, "Audio system initialized (44100 Hz, stereo)", 0.5f});
    state.consoleMessages.push_back({ConsoleMessage::Warning, "NavMesh needs rebake after terrain modification", 0.6f});

    state.assets = {
        "Materials/PBR_Standard.mat", "Materials/Unlit.mat", "Materials/Water.mat",
        "Textures/ground_diffuse.png", "Textures/ground_normal.png", "Textures/player_albedo.png",
        "Meshes/Player.fbx", "Meshes/Enemy.fbx", "Meshes/Weapon.fbx",
        "Scripts/PlayerController.lua", "Scripts/EnemyAI.lua", "Scripts/GameManager.lua",
        "Audio/bgm_battle.ogg", "Audio/sfx_hit.wav", "Audio/sfx_pickup.wav",
        "Shaders/PBR.vert", "Shaders/PBR.frag", "Shaders/PostProcess.frag",
        "Scenes/SampleScene.scene", "Scenes/MainMenu.scene",
        "Prefabs/Enemy.prefab", "Prefabs/Collectible.prefab"
    };

    state.selectedObject = 3;
    state.initialized = true;
}

// ======================== Menu Bar ========================

// Helper: extract filename from full path
static std::string ExtractFilename(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

static void BuildMenuBar(UIBuilder& ui, FluentApp& app, EditorState& state) {
    ui.menuBar([&](UIBuilder& ui) {
        ui.menu("File", [&](UIBuilder& ui) {
            if (ui.menuItem("New Scene")) {
                state.consoleMessages.push_back({ConsoleMessage::Info, "Creating new scene...", state.playTime});
            }
            if (ui.menuItem("Open Scene...")) {
                EditorState* statePtr = &state;
                ShowOpenFileDialog(app.window(),
                    {{"Scene files", "scene;json"}, {"All files", "*"}},
                    "", false,
                    [statePtr](const std::vector<std::string>& paths, int) {
                        std::lock_guard<std::mutex> lock(statePtr->fileDialogMutex);
                        if (!paths.empty()) {
                            statePtr->lastScenePath = paths[0];
                            statePtr->pendingMessages.push_back({ConsoleMessage::Info,
                                "Opened scene: " + ExtractFilename(paths[0]), 0.0f});
                        } else {
                            statePtr->pendingMessages.push_back({ConsoleMessage::Info,
                                "Open scene cancelled", 0.0f});
                        }
                    });
            }
            if (ui.menuItem("Save Scene As...")) {
                EditorState* statePtr = &state;
                ShowSaveFileDialog(app.window(),
                    {{"Scene files", "scene"}, {"JSON", "json"}},
                    state.lastScenePath,
                    [statePtr](const std::vector<std::string>& paths, int) {
                        std::lock_guard<std::mutex> lock(statePtr->fileDialogMutex);
                        if (!paths.empty()) {
                            statePtr->lastScenePath = paths[0];
                            statePtr->pendingMessages.push_back({ConsoleMessage::Info,
                                "Scene saved to: " + ExtractFilename(paths[0]), 0.0f});
                        }
                    });
            }
            ui.menuSeparator();
            if (ui.menuItem("Import Asset...")) {
                EditorState* statePtr = &state;
                ShowOpenFileDialog(app.window(),
                    {{"All supported", "fbx;obj;png;jpg;wav;ogg;lua;scene;mat"},
                     {"3D Models", "fbx;obj;gltf"},
                     {"Textures", "png;jpg;bmp;tga"},
                     {"Audio", "wav;ogg;mp3"},
                     {"Scripts", "lua;py"},
                     {"All files", "*"}},
                    "", true,
                    [statePtr](const std::vector<std::string>& paths, int) {
                        std::lock_guard<std::mutex> lock(statePtr->fileDialogMutex);
                        for (const auto& p : paths) {
                            std::string filename = ExtractFilename(p);
                            statePtr->pendingAssets.push_back(filename);
                            statePtr->pendingMessages.push_back({ConsoleMessage::Info,
                                "Imported asset: " + filename, 0.0f});
                        }
                        if (paths.empty()) {
                            statePtr->pendingMessages.push_back({ConsoleMessage::Info,
                                "Import cancelled", 0.0f});
                        }
                    });
            }
            ui.menuSeparator();
            if (ui.menuItem("Exit")) app.quit();
        });

        ui.menu("Edit", [&](UIBuilder& ui) {
            if (ui.menuItem("Undo", app.undoStack.CanUndo())) {
                app.undoStack.Undo();
            }
            if (ui.menuItem("Redo", app.undoStack.CanRedo())) {
                app.undoStack.Redo();
            }
            ui.menuSeparator();
            if (ui.menuItem("Add Object...")) {
                state.showNewObjectModal = true;
            }
            if (ui.menuItem("Delete Object", state.selectedObject >= 0)) {
                if (state.selectedObject >= 0 && state.selectedObject < (int)state.objects.size()) {
                    std::string name = state.objects[state.selectedObject].name;
                    state.objects.erase(state.objects.begin() + state.selectedObject);
                    state.selectedObject = -1;
                    state.consoleMessages.push_back({ConsoleMessage::Info, "Deleted: " + name, state.playTime});
                }
            }
        });

        ui.menu("View", [&](UIBuilder& ui) {
            if (ui.menuItem("Toggle Grid")) state.showGrid = !state.showGrid;
            if (ui.menuItem("Toggle Wireframe")) state.showWireframe = !state.showWireframe;
            ui.menuSeparator();
            if (ui.menuItem("Debug Overlay (F3)")) {
                app.showDebugOverlay(!app.isDebugOverlayVisible());
            }
            ui.menuSeparator();
            if (ui.menuItem("Dark Theme")) { state.currentTheme = 0; app.enableDarkMode(true); }
            if (ui.menuItem("Light Theme")) { state.currentTheme = 1; app.enableDarkMode(false); }
            if (ui.menuItem("High Contrast")) { state.currentTheme = 2; app.setTheme(GetHighContrastStyle()); }
        });

        ui.menu("Help", [&](UIBuilder& ui) {
            if (ui.menuItem("About")) state.showAboutModal = true;
        });
    });
}

// ======================== Toolbar ========================

static void BuildToolbar(UIBuilder& ui, EditorState& state) {
    ui.toolbar([&](UIBuilder& ui) {
        if (state.isPlaying) {
            ui.pushButtonStyle({
                .background = ColorState{Color(0.8f,0.2f,0.2f), Color(0.9f,0.3f,0.3f), Color(0.7f,0.15f,0.15f)},
                .cornerRadius = 4.0f,
                .text = {12.0f, 0, FontWeight::Bold, Color(1,1,1,1)}
            });
            if (ui.button("Stop", {55, 26})) {
                state.isPlaying = false;
                state.consoleMessages.push_back({ConsoleMessage::Info, "Play mode stopped", state.playTime});
            }
            ui.popButtonStyle();
        } else {
            ui.pushButtonStyle({
                .background = ColorState{Color(0.1f,0.6f,0.2f), Color(0.15f,0.7f,0.25f), Color(0.08f,0.5f,0.15f)},
                .cornerRadius = 4.0f,
                .text = {12.0f, 0, FontWeight::Bold, Color(1,1,1,1)}
            });
            if (ui.button("Play", {55, 26})) {
                state.isPlaying = true;
                state.playTime = 0.0f;
                state.consoleMessages.push_back({ConsoleMessage::Info, "Entering play mode...", state.playTime});
            }
            ui.popButtonStyle();
        }

        ui.sameLine();
        if (ui.button("Pause", {55, 26})) {}
        ui.sameLine();
        ui.separator();
        ui.sameLine();
        if (ui.button("Move", {45, 26})) {}
        ui.sameLine();
        if (ui.button("Rot", {40, 26})) {}
        ui.sameLine();
        if (ui.button("Scl", {40, 26})) {}
        ui.sameLine();
        ui.separator();
        ui.sameLine();
        static bool snap = true;
        ui.checkbox("Snap", &snap);
    });
}

// ======================== Hierarchy (left panel content) ========================

static void BuildHierarchy(UIBuilder& ui, EditorState& state) {
    ui.label("Hierarchy", TypographyStyle::BodyStrong);
    ui.spacing(4);

    static std::string searchFilter;
    ui.textInput("Search", &searchFilter, 120);
    ui.spacing(4);

    ui.treeView("hierarchy_tree", {0, 0}, [&](UIBuilder& ui) {
        for (int i = 0; i < (int)state.objects.size(); ++i) {
            if (state.objects[i].parent != -1) continue;
            if (!searchFilter.empty() && state.objects[i].name.find(searchFilter) == std::string::npos) continue;

            auto& obj = state.objects[i];
            bool hasChildren = !obj.children.empty();

            if (hasChildren) {
                std::function<void(UIBuilder&)> childrenFn = [&](UIBuilder& ui) {
                    for (int childIdx : obj.children) {
                        if (childIdx < 0 || childIdx >= (int)state.objects.size()) continue;
                        if (ui.treeNode(state.objects[childIdx].name)) {
                            state.selectedObject = childIdx;
                        }
                    }
                };
                bool nodeOpen = true;
                if (ui.treeNode(obj.name, &nodeOpen, childrenFn)) {
                    state.selectedObject = i;
                }
            } else {
                if (ui.treeNode(obj.name)) {
                    state.selectedObject = i;
                }
            }
        }
    });

    ui.spacing(8);
    if (ui.button("Add Object")) {
        state.showNewObjectModal = true;
    }
}

// ======================== Inspector (right panel content) ========================

static void BuildInspector(UIBuilder& ui, FluentApp& app, EditorState& state) {
    ui.label("Inspector", TypographyStyle::BodyStrong);
    ui.spacing(4);

    if (state.selectedObject < 0 || state.selectedObject >= (int)state.objects.size()) {
        ui.label("No object selected", TypographyStyle::Caption);
        return;
    }

    auto& obj = state.objects[state.selectedObject];

    ui.textInput("Name", &obj.name, 180);
    ui.spacing(4);

    static std::vector<std::string> tags = {"Untagged", "Player", "Enemy", "Environment", "Light", "MainCamera", "Weapon", "Shield", "Pickup"};
    int tagIdx = 0;
    for (int t = 0; t < (int)tags.size(); ++t) {
        if (tags[t] == obj.tag) { tagIdx = t; break; }
    }
    if (ui.comboBox("Tag", &tagIdx, tags, 180)) {
        obj.tag = tags[tagIdx];
    }

    ui.spacing(8);
    ui.separator();
    ui.spacing(4);

    ui.label("Transform", TypographyStyle::BodyStrong);
    ui.spacing(2);
    ui.dragFloat3("Position", obj.position, 0.1f, -1000.0f, 1000.0f, "%.2f");
    ui.dragFloat3("Rotation", obj.rotation, 1.0f, -360.0f, 360.0f, "%.1f");
    ui.dragFloat3("Scale", obj.scale, 0.01f, 0.01f, 100.0f, "%.3f");

    ui.spacing(8);
    ui.separator();
    ui.spacing(4);

    ui.label("Rendering", TypographyStyle::BodyStrong);
    ui.spacing(2);
    static std::vector<std::string> meshTypes = {"Cube", "Sphere", "Plane", "Cylinder"};
    ui.comboBox("Mesh", &obj.meshType, meshTypes, 180);
    ui.colorPicker("Color", &obj.color);

    ui.spacing(8);
    ui.separator();
    ui.spacing(4);

    ui.label("Properties", TypographyStyle::BodyStrong);
    ui.spacing(2);
    ui.checkbox("Visible", &obj.visible);
    ui.checkbox("Static", &obj.isStatic);
}

// ======================== Viewport (center top) ========================

static void BuildViewport(UIBuilder& ui, EditorState& state) {
    auto* ctx = ui.context();
    if (!ctx) return;

    // Use ALL available space for the viewport
    Vec2 vpPos = ctx->cursorPos;
    Vec2 viewport = ctx->renderer.GetViewportSize();

    // Calculate available space from layout
    float availW = 0, availH = 0;
    if (!ctx->layoutStack.empty()) {
        auto& layout = ctx->layoutStack.back();
        availW = layout.availableSpace.x - 4.0f;
        availH = layout.availableSpace.y - 30.0f; // leave room for zoom slider
    }
    if (availW < 100) availW = viewport.x - vpPos.x - 20.0f;
    if (availH < 100) availH = 300.0f;

    Vec2 vpSize = {availW, availH};

    // Background
    ctx->renderer.DrawRectFilled(vpPos, vpSize, state.viewportBgColor, 2.0f);

    // Grid
    if (state.showGrid) {
        float gridSize = 30.0f * state.viewportZoom;
        Color gridColor(0.25f, 0.25f, 0.3f, 0.25f);
        for (float x = vpPos.x; x < vpPos.x + vpSize.x; x += gridSize)
            ctx->renderer.DrawLine({x, vpPos.y}, {x, vpPos.y + vpSize.y}, gridColor);
        for (float y = vpPos.y; y < vpPos.y + vpSize.y; y += gridSize)
            ctx->renderer.DrawLine({vpPos.x, y}, {vpPos.x + vpSize.x, y}, gridColor);

        float cx = vpPos.x + vpSize.x * 0.5f;
        float cy = vpPos.y + vpSize.y * 0.5f;
        ctx->renderer.DrawLine({cx, vpPos.y}, {cx, vpPos.y + vpSize.y}, Color(0.2f, 0.5f, 0.2f, 0.4f), 1.5f);
        ctx->renderer.DrawLine({vpPos.x, cy}, {vpPos.x + vpSize.x, cy}, Color(0.5f, 0.2f, 0.2f, 0.4f), 1.5f);
    }

    // Scene objects
    float cx = vpPos.x + vpSize.x * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.5f;
    ctx->renderer.PushClipRect(vpPos, vpSize);
    for (int i = 0; i < (int)state.objects.size(); ++i) {
        auto& obj = state.objects[i];
        if (!obj.visible) continue;

        float ox = cx + obj.position[0] * 10.0f * state.viewportZoom;
        float oy = cy - obj.position[2] * 10.0f * state.viewportZoom;
        float r = std::min(20.0f, std::max(3.0f, obj.scale[0] * 5.0f * state.viewportZoom));

        // Selection ring
        if (i == state.selectedObject) {
            ctx->renderer.DrawCircle({ox, oy}, r + 3.0f, Color(1, 0.8f, 0, 0.7f), false);
            ctx->renderer.DrawCircle({ox, oy}, r + 4.0f, Color(1, 0.8f, 0, 0.3f), false);
        }

        if (obj.meshType == 1) {
            ctx->renderer.DrawCircle({ox, oy}, r, obj.color, true);
        } else if (obj.meshType == 2) {
            // Plane = flat wide rect
            ctx->renderer.DrawRectFilled({ox - r*2, oy - 2}, {r*4, 4}, obj.color, 1.0f);
        } else {
            ctx->renderer.DrawRectFilled({ox - r, oy - r}, {r*2, r*2}, obj.color, 2.0f);
        }

        // Label
        ctx->renderer.DrawText({ox - 12.0f, oy + r + 2.0f}, obj.name, Color(0.8f,0.8f,0.8f,0.6f), 8.0f);
    }

    // Bezier decoration
    ctx->renderer.DrawBezier(
        {vpPos.x + 20, vpPos.y + vpSize.y - 30},
        {vpPos.x + vpSize.x * 0.3f, vpPos.y + vpSize.y * 0.4f},
        {vpPos.x + vpSize.x * 0.6f, vpPos.y + vpSize.y * 0.6f},
        {vpPos.x + vpSize.x - 20, vpPos.y + vpSize.y - 30},
        Color(0.3f, 0.5f, 0.8f, 0.15f), 1.5f, 32);

    ctx->renderer.PopClipRect();

    // Border
    ctx->renderer.DrawRect(vpPos, vpSize, Color(0.35f, 0.35f, 0.4f, 0.6f), 2.0f);

    // Advance layout past viewport
    ctx->cursorPos.y = vpPos.y + vpSize.y + 4.0f;
    ctx->lastItemSize = vpSize;

    // Zoom slider below viewport
    ui.slider("Zoom", &state.viewportZoom, 0.25f, 4.0f, 120.0f, "%.0f%%");
}

// ======================== Console (bottom left tab) ========================

static void BuildConsole(UIBuilder& ui, EditorState& state) {
    // Header row
    ui.horizontal([&](UIBuilder& ui) {
        ui.checkbox("Info", &state.showInfo);
        ui.sameLine();
        ui.checkbox("Warn", &state.showWarnings);
        ui.sameLine();
        ui.checkbox("Err", &state.showErrors);
        ui.sameLine();
        if (ui.button("Clear", {45, 22})) state.consoleMessages.clear();
    });

    ui.spacing(2);

    // Messages
    ui.scrollView("console_scroll", {0, 0}, [&](UIBuilder& ui) {
        for (auto& msg : state.consoleMessages) {
            bool show = (msg.level == ConsoleMessage::Info && state.showInfo) ||
                        (msg.level == ConsoleMessage::Warning && state.showWarnings) ||
                        (msg.level == ConsoleMessage::Error && state.showErrors);
            if (!show) continue;

            Color c;
            std::string pfx;
            switch (msg.level) {
                case ConsoleMessage::Info:    c = Color(0.7f,0.8f,0.9f,1); pfx = "[INFO] "; break;
                case ConsoleMessage::Warning: c = Color(1.0f,0.85f,0.3f,1); pfx = "[WARN] "; break;
                case ConsoleMessage::Error:   c = Color(1.0f,0.4f,0.4f,1); pfx = "[ERR]  "; break;
            }
            ui.pushTextColor(c);
            ui.label(pfx + msg.text, TypographyStyle::Caption);
            ui.popTextColor();
        }
    });
}

// ======================== Asset Browser (bottom right tab) ========================

static void BuildAssetBrowser(UIBuilder& ui, EditorState& state) {
    static std::string assetFilter;
    ui.textInput("Filter", &assetFilter, 180);
    ui.spacing(2);

    std::vector<std::string> filtered;
    for (auto& a : state.assets) {
        if (assetFilter.empty() || a.find(assetFilter) != std::string::npos)
            filtered.push_back(a);
    }

    ui.listView("asset_list", {0, 0}, &state.selectedAsset, filtered);
}

// ======================== Modals ========================

static void BuildModals(UIBuilder& ui, EditorState& state) {
    ui.modal("about_modal", "About", &state.showAboutModal, {380, 220}, [&](UIBuilder& ui) {
        ui.label("FluentUI Engine Editor", TypographyStyle::Title);
        ui.spacing(6);
        ui.label("Version 2.0.0");
        ui.label("Built with FluentUI", TypographyStyle::Caption);
        ui.spacing(8);
        ui.label("Hybrid Retained/Immediate Mode GUI", TypographyStyle::Caption);
        ui.label("MSDF Text | Docking | Multi-Window | Undo/Redo", TypographyStyle::Caption);
        ui.spacing(10);
        if (ui.button("Close")) state.showAboutModal = false;
    });

    ui.modal("new_obj_modal", "Add Object", &state.showNewObjectModal, {320, 180}, [&](UIBuilder& ui) {
        ui.textInput("Name", &state.newObjectName, 220);
        ui.spacing(4);
        static std::vector<std::string> meshTypes = {"Cube", "Sphere", "Plane", "Cylinder"};
        ui.comboBox("Mesh", &state.newObjectMesh, meshTypes, 220);
        ui.spacing(10);
        ui.horizontal([&](UIBuilder& ui) {
            if (ui.button("Create", {70, 28})) {
                SceneObject newObj;
                newObj.name = state.newObjectName;
                newObj.meshType = state.newObjectMesh;
                state.objects.push_back(newObj);
                state.selectedObject = (int)state.objects.size() - 1;
                state.consoleMessages.push_back({ConsoleMessage::Info, "Created: " + state.newObjectName, state.playTime});
                state.showNewObjectModal = false;
                state.newObjectName = "NewObject";
            }
            ui.sameLine();
            if (ui.button("Cancel", {70, 28})) state.showNewObjectModal = false;
        });
    });
}

// ======================== Main Build Function ========================

void BuildEditorUI(UIBuilder& ui, FluentApp& app, EditorState& state) {
    InitEditorState(state);

    if (state.isPlaying) state.playTime += ui.context()->deltaTime;

    // Process async file dialog results (thread-safe)
    {
        std::lock_guard<std::mutex> lock(state.fileDialogMutex);
        for (auto& msg : state.pendingMessages) {
            msg.timestamp = state.playTime;
            state.consoleMessages.push_back(std::move(msg));
        }
        state.pendingMessages.clear();

        for (auto& asset : state.pendingAssets) {
            // Avoid duplicates
            if (std::find(state.assets.begin(), state.assets.end(), asset) == state.assets.end()) {
                state.assets.push_back(std::move(asset));
            }
        }
        state.pendingAssets.clear();
    }

    // Process dropped files
    auto* ctx = ui.context();
    if (ctx && ctx->input.HasDroppedFiles()) {
        for (const auto& path : ctx->input.DroppedFiles()) {
            std::string filename = ExtractFilename(path);
            state.consoleMessages.push_back({ConsoleMessage::Info,
                "File dropped: " + filename + " (" + path + ")", state.playTime});
            // Add to asset browser if not already present
            if (std::find(state.assets.begin(), state.assets.end(), filename) == state.assets.end()) {
                state.assets.push_back(filename);
            }
        }
    }

    // Menu bar
    BuildMenuBar(ui, app, state);

    // Toolbar
    BuildToolbar(ui, state);

    // Main layout: Hierarchy | (Viewport / Console) | Inspector
    // Using nested splitters for the full editor layout
    ui.splitter("main_split", true, &state.hierarchySplitRatio,
        // LEFT: Hierarchy
        [&](UIBuilder& ui) {
            ui.scrollView("hierarchy_sv", {0, 0}, [&](UIBuilder& ui) {
                BuildHierarchy(ui, state);
            });
        },
        // RIGHT: (Viewport+Console) | Inspector
        [&](UIBuilder& ui) {
            ui.splitter("center_inspector_split", true, &state.inspectorSplitRatio,
                // CENTER: Viewport / Console tabs
                [&](UIBuilder& ui) {
                    ui.splitter("viewport_console_split", false, &state.viewportConsoleSplitRatio,
                        // TOP: Viewport
                        [&](UIBuilder& ui) {
                            BuildViewport(ui, state);
                        },
                        // BOTTOM: Console/Assets tabs
                        [&](UIBuilder& ui) {
                            static int bottomTab = 0;
                            static std::vector<std::string> tabs = {"Console", "Assets"};
                            ui.tabView("bottom_tabs", &bottomTab, tabs, {0, 0},
                                [&](UIBuilder& ui, int tab) {
                                    if (tab == 0) BuildConsole(ui, state);
                                    else BuildAssetBrowser(ui, state);
                                });
                        }
                    );
                },
                // RIGHT: Inspector
                [&](UIBuilder& ui) {
                    ui.scrollView("inspector_sv", {0, 0}, [&](UIBuilder& ui) {
                        BuildInspector(ui, app, state);
                    });
                }
            );
        }
    );

    // Status bar
    char statusText[256];
    snprintf(statusText, sizeof(statusText), "Objects: %d  |  Selected: %s  |  %s  |  DPI: %.0f%%",
             (int)state.objects.size(),
             state.selectedObject >= 0 ? state.objects[state.selectedObject].name.c_str() : "None",
             state.isPlaying ? "PLAYING" : "EDITOR",
             ui.dpiScale() * 100.0f);
    ui.statusBar(statusText);

    // Modals
    BuildModals(ui, state);
}
