#include "FluentGUI.h"
#include "App.h"
#include "EngineEditor.h"

using namespace FluentUI;

// Set to 1 to run the widget gallery (App) on the standalone Vulkan backend.
// Set to 0 to run the engine-editor example (EngineEditor) via FluentApp — now on
// Vulkan, with multi-window validation (menu Window → "Nueva ventana (compartida)").
#define RUN_WIDGET_GALLERY_VULKAN 1

#if RUN_WIDGET_GALLERY_VULKAN
int main(int, char**) {
    // Standalone test of the Vulkan backend with the full widget gallery.
    SetPreferredBackend(RenderBackendType::Vulkan);
    App app("FluentUI Widget Gallery - Vulkan (standalone)");
    app.Run();
    return 0;
}
#else
int main(int, char**) {
    // gap #3 validation: drive FluentApp::run on Vulkan. Must be set BEFORE the
    // FluentApp ctor creates the window (it picks SDL_WINDOW_VULKAN vs OPENGL from
    // the preferred backend). Comment this line out to fall back to OpenGL.
    SetPreferredBackend(RenderBackendType::Vulkan);

    FluentApp app("FluentUI Engine Editor", {1400, 900, true, true, 60, true});

    EditorState editorState;

    // Keyboard shortcuts
    app.shortcuts.Register("file.save", {UIKey::S, MOD_CTRL}, [&]{
        app.undoStack.Execute({"Save Scene", []{}, []{}});
        editorState.consoleMessages.push_back({ConsoleMessage::Info, "Scene saved (Ctrl+S)", editorState.playTime});
    });

    app.shortcuts.Register("edit.undo", {UIKey::Z, MOD_CTRL}, [&]{
        if (app.undoStack.CanUndo()) app.undoStack.Undo();
    });

    app.shortcuts.Register("edit.redo", {UIKey::Y, MOD_CTRL}, [&]{
        if (app.undoStack.CanRedo()) app.undoStack.Redo();
    });

    app.shortcuts.Register("edit.delete", {UIKey::Delete, 0}, [&]{
        if (editorState.selectedObject >= 0 && editorState.selectedObject < (int)editorState.objects.size()) {
            std::string name = editorState.objects[editorState.selectedObject].name;
            editorState.objects.erase(editorState.objects.begin() + editorState.selectedObject);
            editorState.selectedObject = -1;
            editorState.consoleMessages.push_back({ConsoleMessage::Info, "Deleted: " + name, editorState.playTime});
        }
    });

    app.shortcuts.Register("debug.overlay", {UIKey::F3, 0}, [&]{
        app.showDebugOverlay(!app.isDebugOverlayVisible());
    });

    app.shortcuts.Register("view.play", {UIKey::F5, 0}, [&]{
        editorState.isPlaying = !editorState.isPlaying;
    });

    // Build UI
    app.root([&](UIBuilder& ui) {
        BuildEditorUI(ui, app, editorState);
    });

    app.run();
    return 0;
}
#endif // RUN_WIDGET_GALLERY_VULKAN
