# FluentUI Cookbook

Recetas listas para copiar para los casos más frecuentes.

## 1. Editor de propiedades (inspector)

```cpp
struct Transform { float pos[3]; float rot[3]; float scale[3] = {1,1,1}; };
Transform t;

ui.panel("inspector", [&](FluentUI::UIBuilder& u) {
    u.label("Transform", FluentUI::TypographyStyle::Subtitle);
    u.dragFloat3("Position", t.pos, 0.1f);
    u.dragFloat3("Rotation", t.rot, 0.5f);
    u.dragFloat3("Scale",    t.scale, 0.05f, 0.01f, 100.0f);
});
```

## 2. Consola de debug

```cpp
std::vector<std::string> log;
std::string input;

ui.panel("console", [&](FluentUI::UIBuilder& u) {
    u.scrollView("log", {0, 200}, [&](FluentUI::UIBuilder& sv) {
        for (const auto& line : log) sv.label(line, FluentUI::TypographyStyle::Caption);
    });
    if (u.textInput("> ", &input) && !input.empty()) {
        log.push_back(input);
        input.clear();
    }
});
```

Para autocompletado/historial usa el callback de `TextInput`:

```cpp
FluentUI::TextInput("> ", &input, 220, false, std::nullopt, "command", 0,
    [](FluentUI::TextInputCallbackData& cb) {
        if (cb.type == FluentUI::TextInputCallbackType::Completion) {
            // Tab — autocompletar cb.buffer
        } else if (cb.type == FluentUI::TextInputCallbackType::History) {
            // Up/Down — navegar historial
        }
    },
    static_cast<uint32_t>(FluentUI::TextInputCallbackType::Completion) |
    static_cast<uint32_t>(FluentUI::TextInputCallbackType::History));
```

## 3. Asset browser con drag-drop

```cpp
// Source — el grid del asset browser
for (auto& asset : assets) {
    if (ui.button(asset.name, {80, 80})) selectAsset(asset);
    FluentUI::DragDropSource src("ASSET_PATH");
    if (src.IsActive()) {
        src.SetPayload(asset.path);  // std::string
        src.DragPreview([&]{ FluentUI::Label("📦 " + asset.name); });
    }
}

// Target — la viewport
ui.panel("viewport", [&](FluentUI::UIBuilder& u) {
    FluentUI::DragDropTarget tgt;
    if (tgt.IsHovering()) {
        std::string path;
        if (tgt.AcceptPayload("ASSET_PATH", &path)) {
            spawnEntity(path);
        }
    }
});
```

## 4. Plot dashboard

```cpp
struct Stats { float cpu[128] = {}; float mem[128] = {}; int idx = 0; };
Stats stats;
stats.cpu[stats.idx] = sampleCPU();
stats.mem[stats.idx] = sampleMem();
stats.idx = (stats.idx + 1) % 128;

ui.panel("metrics", [&](FluentUI::UIBuilder&) {
    FluentUI::PlotLines("CPU %",     stats.cpu, 128, stats.idx, "live", 0, 100, {0, 100});
    FluentUI::PlotHistogram("Mem MB",stats.mem, 128, stats.idx, "",     0, 8192, {0, 100});
});
```

## 5. Tabla con freeze columns + multi-row select

```cpp
std::vector<FluentUI::TableColumn> cols = {
    {"ID", 60}, {"Name", 200}, {"Size", 100}, {"Type", 80}
};
static FluentUI::TableState state;
state.frozenColumns = 1;       // Pin "ID"
// state.selectedRows is the multi-select set

if (FluentUI::BeginTable("files", cols, files.size(), {0, 300}, &state)) {
    for (int r = 0; r < (int)files.size(); ++r) {
        FluentUI::TableNextRow();
        FluentUI::TableRowSelectable(r);     // ctrl/shift modifiers handled
        FluentUI::TableSetCell(0); FluentUI::Label(std::to_string(files[r].id));
        FluentUI::TableSetCell(1); FluentUI::Label(files[r].name);
        FluentUI::TableSetCell(2); FluentUI::Label(std::to_string(files[r].size));
        FluentUI::TableSetCell(3); FluentUI::Label(files[r].type);
    }
    FluentUI::EndTable();
}
```

## 6. Tema personalizado

```cpp
auto& s = FluentUI::GetContext()->style;
s = FluentUI::CreateCustomFluentStyle(FluentUI::FluentColors::AccentTeal, /*dark=*/true);
// Ajustes adicionales en runtime
s.spacing = 12.0f;
s.button.cornerRadius = 8.0f;
```

## 7. Hooks de undo/redo via Item state

```cpp
if (ui.dragFloat("X", &t.x)) {
    if (FluentUI::IsItemActivated())          undoStack.push(BeginEdit("transform.x"));
    if (FluentUI::IsItemEdited())             undoStack.top().applyValue(t.x);
    if (FluentUI::IsItemDeactivatedAfterEdit()) undoStack.top().commit();
}
```

## 8. Markup en runtime

```cpp
FluentUI::LabelRich(
    "Estado: <color=#4ade80><b>OK</b></color> · "
    "<a href=\"https://docs/\">Documentación</a> · "
    "<size=11>build " __DATE__ "</size>",
    0.0f, std::nullopt, FluentUI::TypographyStyle::Body,
    [](const std::string& url){ openBrowser(url); });
```

## 9. Ítem inspector / debug

Activa el picker de widgets en cualquier app con **Ctrl+Shift+P** (o llamando
manualmente `FluentUI::Demo::ShowItemPicker()`). Click para congelar la
selección y leer su `id`, bbox y estado de focus en vivo.

## 10. Drag-drop con tipos POD

```cpp
struct EntityRef { uint32_t id; };

// Source
FluentUI::DragDropSource src("ENTITY");
if (src.IsActive()) {
    src.SetPayload(EntityRef{entity.id});  // SetPayload<T> con T trivial
}

// Target
FluentUI::DragDropTarget tgt;
EntityRef out;
if (tgt.AcceptPayload("ENTITY", &out)) attachToParent(out.id);
```
