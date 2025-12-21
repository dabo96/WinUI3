#pragma once
#include "Math/Vec2.h"
#include "UI/Layout.h"
#include "core/Button.h"
#include "core/Element.h"
#include "core/Panel.h"
#include "Theme/Style.h"
#include <string>
#include <vector>
#include <optional>
#include "core/Context.h"

namespace FluentUI {

// Re-export ToastType from Context (or define here if Context not included in user code, but typically it is via Context.h)
// For now, let's assume users can access ToastType via Context or we alias it.
// Actually, it's better to verify if Context.h is exposed. Widgets.h includes Context.h? No, it forward declares or includes it. 
// Let's check imports. Widgets.h includes core/Element.h etc. Context.h is usually internal.
// Let's define the enum in Widgets.h to be safe/clean for the API or use int.
// Using generic ints or re-defining enum in a common place is better. 
// For simplicity in this step, let's alias it or redefine it if Context is included.
// Widgets.cpp includes Context.h. Widgets.h does NOT include Context.h currently.
// Let's modify Widgets.h to include Context.h or define the enum there.
// Context.h includes Widgets.h? No.
// Let's forward declare the enum or move it. 
// To avoid circular deps, let's define ToastType in a new minimal header or just use int, OR include Context.h in Widgets.h.
// Widgets.h currently includes "Theme/Style.h", "core/Button.h" etc. 
// Let's look at Widgets.h again.

// ToastType is now defined in Context.h

// API tipo ImGUI para widgets

// Layout helpers
void BeginVertical(float spacing = -1.0f, std::optional<Vec2> size = std::nullopt, std::optional<Vec2> padding = std::nullopt);
void EndVertical(bool advanceParent = true);

void BeginHorizontal(float spacing = -1.0f, std::optional<Vec2> size = std::nullopt, std::optional<Vec2> padding = std::nullopt);
void EndHorizontal(bool advanceParent = true);

// Layout constraints
void SetNextConstraints(const LayoutConstraints& constraints);

// Widgets básicos
bool Button(const std::string &label, const Vec2 &size = Vec2(0, 0), const Vec2 &pos = Vec2(0, 0), bool enabled = true);
void Label(const std::string &text, std::optional<Vec2> position = std::nullopt, TypographyStyle variant = TypographyStyle::Body, bool disabled = false);
void Separator();
bool Checkbox(const std::string &label, bool *value = nullptr, const Vec2 &pos = Vec2(0, 0));
bool RadioButton(const std::string &label, int *value, int optionValue, const std::string &group = "", const Vec2 &pos = Vec2(0, 0));
bool SliderFloat(const std::string &label, float *value, float minValue, float maxValue, float width = 200.0f, const char *format = "%.2f", const Vec2 &pos = Vec2(0, 0));
bool SliderInt(const std::string &label, int *value, int minValue, int maxValue, float width = 200.0f, const Vec2 &pos = Vec2(0, 0));
void ProgressBar(float fraction, const Vec2 &size = Vec2(0, 0), const std::string &overlay = "", const Vec2 &pos = Vec2(0, 0));
bool TextInput(const std::string &label, std::string *value, float width = 200.0f, bool multiline = false, const Vec2 &pos = Vec2(0, 0));
bool ComboBox(const std::string &label, int *currentItem, const std::vector<std::string> &items, float width = 200.0f, const Vec2 &pos = Vec2(0, 0));
bool ToggleSwitch(const std::string &label, bool *value, const Vec2 &pos = Vec2(0, 0));
bool SpinBox(const std::string &label, int *value, int min, int max, float width = 150.0f, const Vec2 &pos = Vec2(0, 0));

// Game Engine Widgets
void Image(uint32_t textureId, const Vec2 &size, const Vec2 &uv0 = Vec2(0,0), const Vec2 &uv1 = Vec2(1,1), const Color &tintColor = Color(1,1,1,1));
bool DragFloat(const std::string &label, float *value, float speed = 1.0f, float min = 0.0f, float max = 0.0f, float width = 150.0f, const Vec2 &pos = Vec2(0,0));
bool DragVector3(const std::string &label, float *values, float speed = 1.0f, float min = 0.0f, float max = 0.0f, const Vec2 &pos = Vec2(0,0));
bool CollapsibleHeader(const std::string &label, bool defaultOpen = false, const Vec2 &pos = Vec2(0,0));
// New Group Widget
bool BeginCollapsibleGroup(const std::string &label, bool defaultOpen = false, const Vec2 &pos = Vec2(0,0));
void EndCollapsibleGroup();

// Contenedores
bool BeginPanel(const std::string &id, const Vec2 &size = Vec2(0, 0), bool reserveLayoutSpace = true,
                std::optional<bool> useAcrylic = std::nullopt, std::optional<float> acrylicOpacity = std::nullopt, const Vec2 &pos = Vec2(0, 0), bool isDockable = true);
void EndPanel();
bool BeginScrollView(const std::string &id, const Vec2 &size, Vec2 *scrollOffset = nullptr, const Vec2 &pos = Vec2(0, 0));
void EndScrollView();
bool BeginTabView(const std::string &id, int *activeTab, const std::vector<std::string> &tabLabels, const Vec2 &size = Vec2(0, 0), const Vec2 &pos = Vec2(0, 0));
void EndTabView();

// Widgets especializados
void Tooltip(const std::string &text, float delay = 0.5f);
bool BeginContextMenu(const std::string &id);
bool ContextMenuItem(const std::string &label, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();
bool BeginModal(const std::string &id, const std::string &title, bool *open, const Vec2 &size = Vec2(400, 300));
void EndModal();
bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem, const std::vector<std::string> &items, const Vec2 &pos = Vec2(0, 0));
void EndListView();

// Widgets Avanzados
bool Splitter(const std::string &id, bool splitVertically, float thickness, float *size1, float *size2, float minSize1, float minSize2);
bool ColorPicker(const std::string &label, Color *color);

// TreeView structures
struct TreeNode {
    std::string label;
    bool isOpen = false;
    bool isSelected = false;
    std::vector<TreeNode> children;
    
    TreeNode(const std::string& lbl) : label(lbl) {}
};

bool TreeNode(const std::string &id, const std::string &label, bool *isOpen = nullptr, bool *isSelected = nullptr);
void TreeNodePush(); // Incrementar profundidad para hijos
void TreeNodePop();  // Decrementar profundidad después de hijos
bool BeginTreeView(const std::string &id, const Vec2 &size, const Vec2 &pos = Vec2(0, 0));
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

void Spacing(float pixels);
void SameLine(float offset = 0.0f);

// Toast Notifications
void ShowNotification(const std::string &message, ToastType type = ToastType::Info);
void RenderNotifications();

// Docking System
void BeginDockSpace(const std::string &id);
void EndDockSpace();

} // namespace FluentUI
