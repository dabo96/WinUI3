#include "FluentGUI.h"
#include "EngineEditor.h"

using namespace FluentUI;

int main(int, char**) {
    FluentApp app("FluentUI Engine Editor", {1400, 900, true, true, 60, true});

    EditorState editorState;

    // Keyboard shortcuts
    app.shortcuts.Register("file.save", {SDL_SCANCODE_S, MOD_CTRL}, [&]{
        app.undoStack.Execute({"Save Scene", []{}, []{}});
        editorState.consoleMessages.push_back({ConsoleMessage::Info, "Scene saved (Ctrl+S)", editorState.playTime});
    });

    app.shortcuts.Register("edit.undo", {SDL_SCANCODE_Z, MOD_CTRL}, [&]{
        if (app.undoStack.CanUndo()) app.undoStack.Undo();
    });

    app.shortcuts.Register("edit.redo", {SDL_SCANCODE_Y, MOD_CTRL}, [&]{
        if (app.undoStack.CanRedo()) app.undoStack.Redo();
    });

    app.shortcuts.Register("edit.delete", {SDL_SCANCODE_DELETE, 0}, [&]{
        if (editorState.selectedObject >= 0 && editorState.selectedObject < (int)editorState.objects.size()) {
            std::string name = editorState.objects[editorState.selectedObject].name;
            editorState.objects.erase(editorState.objects.begin() + editorState.selectedObject);
            editorState.selectedObject = -1;
            editorState.consoleMessages.push_back({ConsoleMessage::Info, "Deleted: " + name, editorState.playTime});
        }
    });

    app.shortcuts.Register("debug.overlay", {SDL_SCANCODE_F3, 0}, [&]{
        app.showDebugOverlay(!app.isDebugOverlayVisible());
    });

    app.shortcuts.Register("view.play", {SDL_SCANCODE_F5, 0}, [&]{
        editorState.isPlaying = !editorState.isPlaying;
    });

    // Build UI
    app.root([&](UIBuilder& ui) {
        BuildEditorUI(ui, app, editorState);
    });

    app.run();
    return 0;
}
