#include "FluentGUI.h"
#include "App.h"
#include "EngineEditor.h"

using namespace FluentUI;

// Mode selector:
//   RUN_TITLEBAR_DEMO=1              → minimal borderless custom-title-bar demo
//                                      (FluentApp + AppConfig::useCustomTitleBar).
//   else RUN_WIDGET_GALLERY_VULKAN=1 → widget gallery (App, standalone Vulkan).
//   else                            → engine-editor example (FluentApp, multi-window
//                                      via menu Window → "Nueva ventana (compartida)").
#define RUN_TITLEBAR_DEMO 0
#define RUN_WIDGET_GALLERY_VULKAN 1

#if RUN_TITLEBAR_DEMO
int main(int, char**) {
    // Custom window chrome demo — the TitleBar() widget as real window chrome.
    // FluentApp creates a BORDERLESS window and installs the hit-test; the root UI
    // must draw a TitleBar() each frame (drag/resize/caption buttons all work).
    SetPreferredBackend(RenderBackendType::Vulkan);
    AppConfig cfg;
    cfg.width = 960;
    cfg.height = 600;
    cfg.useCustomTitleBar = true;
    FluentApp app("FluentUI - TitleBar demo", cfg);

    app.root([&](UIBuilder&) {
        UIContext* c = GetContext();
        if (c) c->cursorPos = Vec2(0.0f, 0.0f); // chrome starts at the top-left
        // Full-width bar: drag to move, edges resize, caption buttons
        // (minimizar / maximizar-restaurar / cerrar) top-right — via the platform seam.
        TitleBar("demo_titlebar", "FluentUI - TitleBar demo");

        Spacing(24.0f);
        Label("Barra de titulo custom (window chrome)", std::nullopt, TypographyStyle::Title);
        Spacing(8.0f);
        LabelWrapped("Arrastra la barra superior para mover la ventana; los bordes la "
                     "redimensionan. Los botones de arriba-derecha minimizan, "
                     "maximizan/restauran y cierran.", 640.0f);
        Spacing(16.0f);
        if (Button("Un boton de ejemplo")) {}

        Spacing(16.0f);
        // TeachingTip: pulsa el boton para (re)mostrar la coachmark, anclada a el.
        // Vuelve a aparecer cada vez que lo pulsas; se cierra al hacer click fuera.
        static bool tipOpen = false;
        if (Button("Mostrar teaching tip")) tipOpen = true;
        Rect tipTarget(c->lastItemPos, c->lastItemSize);
        TeachingTip("demo_tip", tipTarget, "Nueva funcion",
                    "Este teaching tip aparece anclado al boton y reaparece cada "
                    "vez que lo pulsas.", "Entendido", &tipOpen);
    });

    app.run();
    return 0;
}
#elif RUN_WIDGET_GALLERY_VULKAN
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
