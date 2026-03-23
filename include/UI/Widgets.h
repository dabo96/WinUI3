#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include "UI/Layout.h"
#include "Theme/Style.h"
#include <string>
#include <vector>
#include <optional>

namespace FluentUI {

/// @defgroup Widgets Immediate-Mode Widget API
/// @brief ImGui-style Begin/End widget API implementing Fluent Design.
/// @{

// ─── Layout ─────────────────────────────────────────────────────────────────

/// Begin a vertical layout group.
/// @param spacing  Pixel spacing between children (-1 = use style default).
/// @param size     Explicit size; std::nullopt = auto from parent.
/// @param padding  Inner padding; std::nullopt = use style default.
void BeginVertical(float spacing = -1.0f, std::optional<Vec2> size = std::nullopt, std::optional<Vec2> padding = std::nullopt);
void EndVertical(bool advanceParent = true);

/// Begin a horizontal layout group.
void BeginHorizontal(float spacing = -1.0f, std::optional<Vec2> size = std::nullopt, std::optional<Vec2> padding = std::nullopt);
void EndHorizontal(bool advanceParent = true);

/// Set size constraints for the next widget (Fixed / Fill / Auto).
void SetNextConstraints(const LayoutConstraints& constraints);

// ─── Basic Widgets ──────────────────────────────────────────────────────────

/// Clickable button. @return true on the frame the button is clicked.
bool Button(const std::string &label, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt, bool enabled = true);

/// Static text label.
/// @param variant Typography style (Body, Title, Caption, etc.).
void Label(const std::string &text, std::optional<Vec2> position = std::nullopt, TypographyStyle variant = TypographyStyle::Body, bool disabled = false);

/// Horizontal line separator.
void Separator();

/// Checkbox toggle. @return true when value changes.
bool Checkbox(const std::string &label, bool *value = nullptr, std::optional<Vec2> pos = std::nullopt);

/// Radio button. @return true when this option becomes selected.
bool RadioButton(const std::string &label, int *value, int optionValue, const std::string &group = "", std::optional<Vec2> pos = std::nullopt);

/// Float slider. @return true when value changes.
bool SliderFloat(const std::string &label, float *value, float minValue, float maxValue, float width = 200.0f, const char *format = "%.2f", std::optional<Vec2> pos = std::nullopt);

/// Integer slider. @return true when value changes.
bool SliderInt(const std::string &label, int *value, int minValue, int maxValue, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

/// Determinate progress bar.
/// @param fraction 0.0 – 1.0 progress.
/// @param overlay  Optional text drawn on top.
void ProgressBar(float fraction, const Vec2 &size = Vec2(0, 0), const std::string &overlay = "", std::optional<Vec2> pos = std::nullopt);

/// Single- or multi-line text input. Supports Ctrl+Z/Y undo, IME, click-drag selection.
/// @param placeholder  Dimmed hint text shown when empty and unfocused.
/// @param maxLength    Maximum number of characters (0 = unlimited).
/// @return true when the text changes.
bool TextInput(const std::string &label, std::string *value, float width = 200.0f, bool multiline = false, std::optional<Vec2> pos = std::nullopt, const char* placeholder = nullptr, size_t maxLength = 0);

/// Dropdown combo box with keyboard navigation (Up/Down/Enter/Escape).
/// @return true when the selected index changes.
bool ComboBox(const std::string &label, int *currentItem, const std::vector<std::string> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

// ─── Drag Widgets ───────────────────────────────────────────────────────────

/// Click-drag float editor. Double-click for keyboard input. @return true when value changes.
bool DragFloat(const std::string& label, float* value, float speed = 1.0f,
               float min = 0.0f, float max = 0.0f, const char* format = "%.2f",
               std::optional<Vec2> pos = std::nullopt);

/// Click-drag integer editor. @return true when value changes.
bool DragInt(const std::string& label, int* value, float speed = 1.0f,
             int min = 0, int max = 0, std::optional<Vec2> pos = std::nullopt);

/// Three-component float drag (e.g. for XYZ vectors). @return true when any value changes.
bool DragFloat3(const std::string& label, float values[3], float speed = 1.0f,
                float min = 0.0f, float max = 0.0f, const char* format = "%.2f",
                std::optional<Vec2> pos = std::nullopt);

// ─── Containers ─────────────────────────────────────────────────────────────

/// Draggable, collapsible panel. @return true if the panel is visible (not minimized).
bool BeginPanel(const std::string &id, const Vec2 &size = Vec2(0, 0), bool reserveLayoutSpace = true,
                std::optional<bool> useAcrylic = std::nullopt, std::optional<float> acrylicOpacity = std::nullopt,
                std::optional<Vec2> pos = std::nullopt, float maxHeight = 0.0f);
void EndPanel();

/// Scrollable region. Pass size {0,0} to fill available space. @return true always.
bool BeginScrollView(const std::string &id, const Vec2 &size, Vec2 *scrollOffset = nullptr, std::optional<Vec2> pos = std::nullopt);
void EndScrollView();

/// Tabbed container. @return true always.
bool BeginTabView(const std::string &id, int *activeTab, const std::vector<std::string> &tabLabels, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt);
void EndTabView();

// ─── Splitter ───────────────────────────────────────────────────────────────

/// Resizable two-panel splitter.
/// @param vertical  true = vertical divider (left|right), false = horizontal (top/bottom).
/// @param ratio     Split ratio (0.0 – 1.0), updated on drag.
bool BeginSplitter(const std::string& id, bool vertical, float* ratio, const Vec2& size = Vec2(0,0));

/// Call between first and second panel content.
void SplitterPanel();
void EndSplitter();

// ─── Overlays ───────────────────────────────────────────────────────────────

/// Show tooltip on hover over the previous widget. Supports multi-line (\\n).
void Tooltip(const std::string &text, float delay = 0.5f);

/// Right-click context menu. @return true if the menu is open.
bool BeginContextMenu(const std::string &id);
/// @return true when clicked.
bool ContextMenuItem(const std::string &label, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();

/// Modal dialog. @return true if visible.
bool BeginModal(const std::string &id, const std::string &title, bool *open, const Vec2 &size = Vec2(400, 300));
void EndModal();

// ─── Lists ──────────────────────────────────────────────────────────────────

/// Single-selection list view.
bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem, const std::vector<std::string> &items, std::optional<Vec2> pos = std::nullopt);
/// Multi-selection list view (Ctrl+Click, Shift+Click).
bool BeginListView(const std::string &id, const Vec2 &size, std::vector<int> *selectedItems, const std::vector<std::string> &items, std::optional<Vec2> pos = std::nullopt);
void EndListView();

// ─── TreeView ───────────────────────────────────────────────────────────────

/// Helper struct for declarative tree construction.
struct TreeNodeData {
    std::string label;
    bool isOpen = false;
    bool isSelected = false;
    std::vector<TreeNodeData> children;

    TreeNodeData(const std::string& lbl) : label(lbl) {}
};

/// Single tree node. @return true if the node is expanded and has children.
/// @param isOpen     Pointer to open state (pass non-null for expandable nodes).
/// @param isSelected Pointer to selection state.
bool TreeNode(const std::string &id, const std::string &label, bool *isOpen = nullptr, bool *isSelected = nullptr);
/// Increase tree depth for child nodes.
void TreeNodePush();
/// Decrease tree depth.
void TreeNodePop();
/// Begin a tree view container.
bool BeginTreeView(const std::string &id, const Vec2 &size, std::optional<Vec2> pos = std::nullopt);
void EndTreeView();

// ─── Menu System ────────────────────────────────────────────────────────────

/// Begin a menu bar (typically at the top of the window).
bool BeginMenuBar();
/// Begin a top-level or nested menu. @return true if the menu is open.
bool BeginMenu(const std::string &label, bool enabled = true);
/// Menu item. @return true when clicked.
bool MenuItem(const std::string &label, bool enabled = true);
void MenuSeparator();
void EndMenu();
void EndMenuBar();

/// Horizontal toolbar strip.
void BeginToolbar();
void EndToolbar();
/// Status bar at the bottom.
void BeginStatusBar(const std::string& text = "");
void EndStatusBar();

/// Render deferred overlays (dropdowns, tooltips). Call before Render().
void RenderDeferredDropdowns();

// ─── Grid ───────────────────────────────────────────────────────────────────

/// Grid layout with fixed column count.
void BeginGrid(const std::string& id, int columns, float rowHeight = 0.0f);
void GridNextCell();
void EndGrid();

// ─── Table / DataGrid ───────────────────────────────────────────────────────

/// Column definition for Table widget.
struct TableColumn {
  std::string header;
  float width = 100.0f;     // Initial/current width
  float minWidth = 40.0f;
  bool sortable = true;
};

struct TableState {
  int sortColumn = -1;       // -1 = no sort
  bool sortAscending = true;
  float scrollOffset = 0.0f;
};
/// Sortable, resizable data table. @return true always.
bool BeginTable(const std::string& id, std::vector<TableColumn>& columns,
                int rowCount, const Vec2& size = Vec2(0, 0),
                TableState* state = nullptr);
void TableNextRow();
void TableSetCell(int column);
void EndTable();

// ─── Specialized Widgets ────────────────────────────────────────────────────

/// HSV color picker with RGB sliders and hex input. @return true when value changes.
bool ColorPicker(const std::string& label, Color* value, std::optional<Vec2> pos = std::nullopt);

/// Display a GPU texture. @param textureHandle Backend-specific texture (e.g. GLuint cast to void*).
void Image(const std::string& id, void* textureHandle, const Vec2& size,
           const Vec2& uv0 = Vec2(0,0), const Vec2& uv1 = Vec2(1,1),
           std::optional<Vec2> pos = std::nullopt);

// ─── Spacing & Cursor ───────────────────────────────────────────────────────

/// Insert vertical spacing.
void Spacing(float pixels);
/// Place the next widget on the same line as the previous one.
void SameLine(float offset = 0.0f);

// ─── DPI ────────────────────────────────────────────────────────────────────

/// Get the current display scale factor (1.0 = 100%).
float GetDPIScale();
/// Scale a pixel value by the current DPI factor.
float Scaled(float value);

// ─── Style Overrides ────────────────────────────────────────────────────────

/// Push a full style override onto the stack.
void PushStyle(const Style& override);
void PopStyle();
void PushButtonStyle(const ButtonStyle& s);
void PopButtonStyle();
void PushPanelStyle(const PanelStyle& s);
void PopPanelStyle();
void PushTextColor(const Color& color);
void PopTextColor();

// ─── Accessibility ──────────────────────────────────────────────────────────

/// Draw a 2px focus ring around a widget for keyboard navigation.
void DrawAccessibilityFocusRing(const Vec2& pos, const Vec2& size);

/// @}

} // namespace FluentUI
