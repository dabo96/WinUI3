#pragma once
#include "FluentGUI.h"
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <mutex>

using namespace FluentUI;

// Simulated scene object
struct SceneObject {
    std::string name;
    float position[3] = {0, 0, 0};
    float rotation[3] = {0, 0, 0};
    float scale[3] = {1, 1, 1};
    Color color = Color(1, 1, 1, 1);
    bool visible = true;
    bool isStatic = false;
    int meshType = 0; // 0=Cube, 1=Sphere, 2=Plane, 3=Cylinder
    std::string tag = "Untagged";
    std::vector<int> children;
    int parent = -1;
};

struct ConsoleMessage {
    enum Level { Info, Warning, Error };
    Level level;
    std::string text;
    float timestamp;
};

// Engine Editor state
struct EditorState {
    // Scene
    std::vector<SceneObject> objects;
    int selectedObject = -1;

    // Console
    std::vector<ConsoleMessage> consoleMessages;
    bool showInfo = true;
    bool showWarnings = true;
    bool showErrors = true;
    std::string consoleInput;

    // Asset browser
    std::vector<std::string> assets;
    int selectedAsset = -1;

    // Viewport
    Color viewportBgColor = Color(0.15f, 0.15f, 0.2f, 1.0f);
    float viewportZoom = 1.0f;
    bool showGrid = true;
    bool showWireframe = false;

    // Editor state
    bool showAboutModal = false;
    bool showNewObjectModal = false;
    std::string newObjectName = "NewObject";
    int newObjectMesh = 0;
    bool isPlaying = false;
    float playTime = 0.0f;

    // File dialog results (thread-safe, written from SDL callback thread)
    std::mutex fileDialogMutex;
    std::vector<ConsoleMessage> pendingMessages;
    std::vector<std::string> pendingAssets;
    std::string lastScenePath;

    // Theme
    int currentTheme = 0; // 0=Dark, 1=Light, 2=HighContrast
    int accentColor = 0;  // 0=Blue, 1=Green, 2=Purple, 3=Orange, 4=Pink, 5=Teal

    // Layout
    float hierarchySplitRatio = 0.2f;
    float inspectorSplitRatio = 0.75f;
    float viewportConsoleSplitRatio = 0.7f;

    // Initialized flag
    bool initialized = false;
};

void InitEditorState(EditorState& state);
void BuildEditorUI(UIBuilder& ui, FluentApp& app, EditorState& state);
