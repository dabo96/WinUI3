#pragma once
#include "Math/Vec2.h"
#include "UI/Layout.h"
#include "Theme/Style.h"
#include <string>
#include <vector>
#include <optional>

namespace FluentUI {

// API tipo ImGUI para widgets

// Layout helpers
void BeginVertical(float spacing = -1.0f, std::optional<Vec2> size = std::nullopt, std::optional<Vec2> padding = std::nullopt);
void EndVertical(bool advanceParent = true);

void BeginHorizontal(float spacing = -1.0f, std::optional<Vec2> size = std::nullopt, std::optional<Vec2> padding = std::nullopt);
void EndHorizontal(bool advanceParent = true);

// Layout constraints
void SetNextConstraints(const LayoutConstraints& constraints);

// Widgets básicos
bool Button(const std::string &label, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt, bool enabled = true);
void Label(const std::string &text, std::optional<Vec2> position = std::nullopt, TypographyStyle variant = TypographyStyle::Body, bool disabled = false);
void Separator();
bool Checkbox(const std::string &label, bool *value = nullptr, std::optional<Vec2> pos = std::nullopt);
bool RadioButton(const std::string &label, int *value, int optionValue, const std::string &group = "", std::optional<Vec2> pos = std::nullopt);
bool SliderFloat(const std::string &label, float *value, float minValue, float maxValue, float width = 200.0f, const char *format = "%.2f", std::optional<Vec2> pos = std::nullopt);
bool SliderInt(const std::string &label, int *value, int minValue, int maxValue, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);
void ProgressBar(float fraction, const Vec2 &size = Vec2(0, 0), const std::string &overlay = "", std::optional<Vec2> pos = std::nullopt);
bool TextInput(const std::string &label, std::string *value, float width = 200.0f, bool multiline = false, std::optional<Vec2> pos = std::nullopt);
bool ComboBox(const std::string &label, int *currentItem, const std::vector<std::string> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

// Contenedores
bool BeginPanel(const std::string &id, const Vec2 &size = Vec2(0, 0), bool reserveLayoutSpace = true,
                std::optional<bool> useAcrylic = std::nullopt, std::optional<float> acrylicOpacity = std::nullopt, std::optional<Vec2> pos = std::nullopt);
void EndPanel();
bool BeginScrollView(const std::string &id, const Vec2 &size, Vec2 *scrollOffset = nullptr, std::optional<Vec2> pos = std::nullopt);
void EndScrollView();
bool BeginTabView(const std::string &id, int *activeTab, const std::vector<std::string> &tabLabels, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt);
void EndTabView();

// Widgets especializados
void Tooltip(const std::string &text, float delay = 0.5f);
bool BeginContextMenu(const std::string &id);
bool ContextMenuItem(const std::string &label, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();
bool BeginModal(const std::string &id, const std::string &title, bool *open, const Vec2 &size = Vec2(400, 300));
void EndModal();
bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem, const std::vector<std::string> &items, std::optional<Vec2> pos = std::nullopt);
void EndListView();

// TreeView structures
struct TreeNodeData {
    std::string label;
    bool isOpen = false;
    bool isSelected = false;
    std::vector<TreeNodeData> children;

    TreeNodeData(const std::string& lbl) : label(lbl) {}
};

bool TreeNode(const std::string &id, const std::string &label, bool *isOpen = nullptr, bool *isSelected = nullptr);
void TreeNodePush(); // Incrementar profundidad para hijos
void TreeNodePop();  // Decrementar profundidad después de hijos
bool BeginTreeView(const std::string &id, const Vec2 &size, std::optional<Vec2> pos = std::nullopt);
void EndTreeView();

// MenuBar
bool BeginMenuBar();
bool BeginMenu(const std::string &label, bool enabled = true);
bool MenuItem(const std::string &label, bool enabled = true);
void MenuSeparator();
void EndMenu();
void EndMenuBar();

// Deferred rendering - call before Render()
void RenderDeferredDropdowns();

// Helpers de espaciado
void Spacing(float pixels);
void SameLine(float offset = 0.0f);

} // namespace FluentUI
