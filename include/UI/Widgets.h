#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include "UI/Layout.h"
#include "UI/Icons.h"
#include "Theme/Style.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <functional>

namespace FluentUI {

// ─── TextInput Callbacks ────────────────────────────────────────────────────

/// Type of TextInput callback event. Use bitwise OR for the callbackMask param.
enum class TextInputCallbackType : uint32_t {
    None       = 0,
    Edit       = 1 << 0, ///< Fires whenever the buffer changes due to editing.
    Completion = 1 << 1, ///< Fires when Tab is pressed (autocompletion hook).
    History    = 1 << 2, ///< Fires when Up/Down arrow pressed (history hook).
    CharFilter = 1 << 3, ///< Fires per character; modify or zero charInput to filter.
    Always     = 1 << 4, ///< Fires every frame the widget is focused.
};

inline uint32_t operator|(TextInputCallbackType a, TextInputCallbackType b) {
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}
inline uint32_t operator|(uint32_t a, TextInputCallbackType b) {
    return a | static_cast<uint32_t>(b);
}
inline uint32_t operator|(TextInputCallbackType a, uint32_t b) {
    return static_cast<uint32_t>(a) | b;
}

struct TextInputCallbackData {
    TextInputCallbackType type = TextInputCallbackType::None;
    std::string* buffer = nullptr;       ///< Mutable buffer (Edit/Completion/History/Always).
    size_t cursorPos = 0;                ///< Current caret position (mutable).
    size_t selectionStart = 0;           ///< Selection start (SIZE_MAX = no selection).
    size_t selectionEnd = 0;             ///< Selection end (SIZE_MAX = no selection).
    uint32_t key = 0;                    ///< For History (SDL scancode of Up/Down) and Completion (Tab).
    uint32_t charInput = 0;              ///< For CharFilter: codepoint received (set to 0 to reject).
};

using TextInputCallback = std::function<void(TextInputCallbackData&)>;

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

/// Fluent 2 button size variants (control the auto-computed height/padding).
///   Small  → ~24px tall, padding 8×3, font 12 (caption)
///   Medium → ~32px tall, padding 12×5, font 14 (body) — default
///   Large  → ~40px tall, padding 16×8, font 16
enum class ButtonSize { Small, Medium, Large };

/// Clickable button. @return true on the frame the button is clicked.
bool Button(const std::string &label, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt, bool enabled = true);

/// Sized variant: applies Fluent 2 small/medium/large preset (overrides theme padding/font).
bool Button(const std::string &label, ButtonSize variant, std::optional<Vec2> pos = std::nullopt, bool enabled = true);

/// Button with a leading icon glyph (icon font, see LoadIconFont). Pass label="" for an
/// icon-only square button (toolbar style). @param iconCodepoint 0 = no icon.
bool Button(const std::string &label, uint32_t iconCodepoint, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt, bool enabled = true);

/// Sized + iconified variant.
bool Button(const std::string &label, uint32_t iconCodepoint, ButtonSize variant, std::optional<Vec2> pos = std::nullopt, bool enabled = true);

/// Toolbar-style icon-only square button. Equivalent to Button("", iconCp) but auto-sizes
/// to a square based on the current button style.
/// @param size Side length in px (0 = derived from current style: fontSize + padding*2).
bool IconButton(uint32_t iconCodepoint, float size = 0.0f, std::optional<Vec2> pos = std::nullopt, bool enabled = true);

/// Segmented control (Fluent 2 TabList horizontal compacto).
/// One option is selected at a time. Useful for tool/mode toggles like Move/Rotate/Scale.
/// @param activeIndex pointer to int holding/receiving the selected index.
/// @return true on the frame the selection changes.
bool SegmentedControl(const std::string &id,
                      const std::vector<std::string> &options,
                      int *activeIndex,
                      std::optional<Vec2> pos = std::nullopt);

/// Segmented control with per-segment icons. Each pair = (label, iconCodepoint).
/// Pass label="" for an icon-only segment (Move/Rotate/Scale toolbars). 0 = no icon.
bool SegmentedControl(const std::string &id,
                      const std::vector<std::pair<std::string, uint32_t>> &options,
                      int *activeIndex,
                      std::optional<Vec2> pos = std::nullopt);

/// Static text label.
/// @param variant Typography style (Body, Title, Caption, etc.).
void Label(const std::string &text, std::optional<Vec2> position = std::nullopt, TypographyStyle variant = TypographyStyle::Body, bool disabled = false);

/// Label with a leading icon glyph. @param iconCodepoint 0 = no icon.
void Label(const std::string &text, uint32_t iconCodepoint, std::optional<Vec2> position = std::nullopt, TypographyStyle variant = TypographyStyle::Body, bool disabled = false);

/// Standalone icon (no label). Useful for inline status badges.
/// @param size Glyph size in px (0 = current Body fontSize).
/// @param color Icon color; std::nullopt = inherit current text color.
void IconLabel(uint32_t iconCodepoint, float size = 0.0f, std::optional<Color> color = std::nullopt, std::optional<Vec2> position = std::nullopt);

/// Word-wrapped label. Greedy wraps at spaces.
/// @param maxWidth  Pixel width to wrap at; 0 = use current layout's available width.
void LabelWrapped(const std::string &text, float maxWidth = 0.0f, std::optional<Vec2> position = std::nullopt, TypographyStyle variant = TypographyStyle::Body, bool disabled = false);

/// Phase C8: Rich-text label with inline markup. Supported tags:
///   <b>...</b>                bold (uses bold variant if available)
///   <i>...</i>                italic (uses italic variant if available)
///   <color=#RRGGBB>...</color>   override color until close tag
///   <color=#RRGGBBAA>...</color> with alpha
///   <size=N>...</size>        override font size
///   <a href="url">...</a>     clickable link; fires onLinkClicked(url)
/// Greedy word-wrap honoured when maxWidth > 0.
void LabelRich(const std::string &markup, float maxWidth = 0.0f,
               std::optional<Vec2> position = std::nullopt,
               TypographyStyle variant = TypographyStyle::Body,
               std::function<void(const std::string &url)> onLinkClicked = nullptr);

// ─── Plot Widgets (Phase C2) ────────────────────────────────────────────────

/// Line plot of a value series. Hover shows the value at the cursor position.
/// @param values    Pointer to a contiguous array of floats.
/// @param count     Number of samples.
/// @param offset    Index of the first sample (for ring buffers).
/// @param overlay   Optional centered overlay text (e.g. "avg: 42.0").
/// @param minScale  Y-axis lower bound (FLT_MAX = auto).
/// @param maxScale  Y-axis upper bound (FLT_MAX = auto).
/// @param size      Widget size (0,0 = auto from layout).
void PlotLines(const std::string &label, const float *values, int count,
               int offset = 0, const std::string &overlay = "",
               float minScale = 3.402823466e+38F, float maxScale = 3.402823466e+38F,
               const Vec2 &size = Vec2(0, 60));

/// Histogram (bar) plot of a value series. Same parameters as PlotLines.
void PlotHistogram(const std::string &label, const float *values, int count,
                   int offset = 0, const std::string &overlay = "",
                   float minScale = 3.402823466e+38F, float maxScale = 3.402823466e+38F,
                   const Vec2 &size = Vec2(0, 60));

/// Compact line plot without label/grid (e.g. inline trend indicator).
void Sparkline(const float *values, int count, const Vec2 &size = Vec2(80, 16));

// ─── Date/Time Pickers (Phase C3) ───────────────────────────────────────────

struct DateTimeValue {
    int year = 1970;   ///< Full year (e.g., 2026).
    int month = 1;     ///< 1-12.
    int day = 1;       ///< 1-31.
    int hour = 0;      ///< 0-23.
    int minute = 0;    ///< 0-59.
    int second = 0;    ///< 0-59.
};

/// Calendar date picker with month/year navigation and 7×6 day grid.
/// @return true when the selected date changes.
bool DatePicker(const std::string &label, DateTimeValue *value, std::optional<Vec2> pos = std::nullopt);

/// Time picker (HH:MM:SS spinners).
/// @return true when a component changes.
bool TimePicker(const std::string &label, DateTimeValue *value, std::optional<Vec2> pos = std::nullopt);

/// Combined date + time picker.
/// @return true when any component changes.
bool DateTimePicker(const std::string &label, DateTimeValue *value, std::optional<Vec2> pos = std::nullopt);

/// Horizontal line separator.
void Separator();

/// Checkbox toggle. @return true when value changes.
bool Checkbox(const std::string &label, bool *value = nullptr, std::optional<Vec2> pos = std::nullopt);

/// Checkbox with a trailing icon glyph (drawn after the label). @param iconCodepoint 0 = no icon.
bool Checkbox(const std::string &label, uint32_t iconCodepoint, bool *value = nullptr, std::optional<Vec2> pos = std::nullopt);

/// Radio button. @return true when this option becomes selected.
bool RadioButton(const std::string &label, int *value, int optionValue, const std::string &group = "", std::optional<Vec2> pos = std::nullopt);

/// Radio button with a trailing icon glyph. @param iconCodepoint 0 = no icon.
bool RadioButton(const std::string &label, uint32_t iconCodepoint, int *value, int optionValue, const std::string &group = "", std::optional<Vec2> pos = std::nullopt);

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

/// TextInput overload accepting callbacks for editing/completion/history/charFilter hooks.
/// @param callback     Invoked when one of the unmasked events fires; may mutate buffer/cursorPos.
/// @param callbackMask Bitwise OR of TextInputCallbackType values to subscribe to.
bool TextInput(const std::string &label, std::string *value, float width, bool multiline, std::optional<Vec2> pos, const char* placeholder, size_t maxLength, const TextInputCallback& callback, uint32_t callbackMask);

/// Dropdown combo box with keyboard navigation (Up/Down/Enter/Escape).
/// @return true when the selected index changes.
bool ComboBox(const std::string &label, int *currentItem, const std::vector<std::string> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

/// ComboBox with per-item icons. Each pair = (label, iconCodepoint).
/// Icon is drawn left of the label both in the closed field (for the selected
/// item) and in every dropdown row. Pass 0 for items that should have no icon.
bool ComboBox(const std::string &label, int *currentItem, const std::vector<std::pair<std::string, uint32_t>> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

/// ComboBox variant with inline search field. When opened, an editable filter
/// row is shown above the items; case-insensitive substring match narrows the list.
bool ComboBoxSearchable(const std::string &label, int *currentItem, const std::vector<std::string> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

/// Searchable ComboBox with per-item icons.
bool ComboBoxSearchable(const std::string &label, int *currentItem, const std::vector<std::pair<std::string, uint32_t>> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

/// Header-less ComboBox: identical behavior but no label/header is drawn above
/// the field. The first parameter is used purely as a unique widget id, so it
/// must be unique within the same scope (it is never displayed).
bool ComboBoxNoLabel(const std::string &id, int *currentItem, const std::vector<std::string> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

/// Header-less ComboBox with per-item icons.
bool ComboBoxNoLabel(const std::string &id, int *currentItem, const std::vector<std::pair<std::string, uint32_t>> &items, float width = 200.0f, std::optional<Vec2> pos = std::nullopt);

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

/// Panel with a leading icon glyph in its title bar. @param iconCodepoint 0 = no icon.
bool BeginPanel(const std::string &id, uint32_t iconCodepoint, const Vec2 &size = Vec2(0, 0), bool reserveLayoutSpace = true,
                std::optional<bool> useAcrylic = std::nullopt, std::optional<float> acrylicOpacity = std::nullopt,
                std::optional<Vec2> pos = std::nullopt, float maxHeight = 0.0f);
void EndPanel();

/// Collapsible section header (Inspector-style). Returns true if expanded; render
/// child widgets only when true. Subsequent widgets in the same vertical layout are
/// auto-indented while expanded; the indent resets on the next CollapsingHeader at
/// the same level or when the parent layout ends. No EndCollapsingHeader needed.
/// @param open Optional external state pointer; if null, internal state is used.
/// @param iconCodepoint Optional Lucide glyph drawn between chevron and label (0 = none).
bool CollapsingHeader(const std::string &label, bool *open = nullptr,
                      uint32_t iconCodepoint = 0,
                      std::optional<Vec2> pos = std::nullopt);

/// Scrollable region. Pass size {0,0} to fill available space. @return true always.
bool BeginScrollView(const std::string &id, const Vec2 &size, Vec2 *scrollOffset = nullptr, std::optional<Vec2> pos = std::nullopt);
void EndScrollView();

/// Tabbed container. @return true always.
bool BeginTabView(const std::string &id, int *activeTab, const std::vector<std::string> &tabLabels, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt);

/// Tabbed container with per-tab icons. Each pair = (label, iconCodepoint); 0 = no icon for that tab.
bool BeginTabView(const std::string &id, int *activeTab, const std::vector<std::pair<std::string, uint32_t>> &tabLabels, const Vec2 &size = Vec2(0, 0), std::optional<Vec2> pos = std::nullopt);
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
/// Context menu item with a leading icon glyph. @param iconCodepoint 0 = no icon.
bool ContextMenuItem(const std::string &label, uint32_t iconCodepoint, bool enabled = true);
void ContextMenuSeparator();
void EndContextMenu();

/// Modal dialog. @return true if visible.
bool BeginModal(const std::string &id, const std::string &title, bool *open, const Vec2 &size = Vec2(400, 300));

/// Modal dialog with a leading icon glyph in its header. @param iconCodepoint 0 = no icon.
bool BeginModal(const std::string &id, const std::string &title, uint32_t iconCodepoint, bool *open, const Vec2 &size = Vec2(400, 300));
void EndModal();

// ─── Lists ──────────────────────────────────────────────────────────────────

/// Single-selection list view.
bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem, const std::vector<std::string> &items, std::optional<Vec2> pos = std::nullopt);
/// Single-selection list view with per-row icons.
bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem, const std::vector<std::pair<std::string, uint32_t>> &items, std::optional<Vec2> pos = std::nullopt);
/// Multi-selection list view (Ctrl+Click, Shift+Click).
bool BeginListView(const std::string &id, const Vec2 &size, std::vector<int> *selectedItems, const std::vector<std::string> &items, std::optional<Vec2> pos = std::nullopt);
/// Multi-selection list view with per-row icons.
bool BeginListView(const std::string &id, const Vec2 &size, std::vector<int> *selectedItems, const std::vector<std::pair<std::string, uint32_t>> &items, std::optional<Vec2> pos = std::nullopt);
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

/// Tree node with leading icon glyph (drawn after the chevron). @param iconCodepoint 0 = no icon.
bool TreeNode(const std::string &id, const std::string &label, uint32_t iconCodepoint, bool *isOpen = nullptr, bool *isSelected = nullptr);

/// Multi-select TreeNode with range/toggle semantics (Phase C5).
/// @param nodeId       Stable integer id of this node (caller assigns; used for selection set).
/// @param selectedIds  Mutable list of selected nodeIds; updated based on modifier:
///                       - Click          → replace with [nodeId]
///                       - Ctrl+Click     → toggle nodeId
///                       - Shift+Click    → range from anchor to nodeId in DFS visit order
/// @return true on the frame the user clicked the node.
bool TreeNodeMulti(const std::string &id, const std::string &label, int nodeId,
                   bool *isOpen, std::vector<int> *selectedIds);

/// Multi-select TreeNode with leading icon glyph. @param iconCodepoint 0 = no icon.
bool TreeNodeMulti(const std::string &id, const std::string &label, uint32_t iconCodepoint,
                   int nodeId, bool *isOpen, std::vector<int> *selectedIds);
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
/// Begin a menu with a leading icon glyph (from the icon font). @param iconCodepoint 0 = no icon.
bool BeginMenu(const std::string &label, uint32_t iconCodepoint, bool enabled = true);
/// Menu item. @return true when clicked.
bool MenuItem(const std::string &label, bool enabled = true);
/// Menu item with a leading icon glyph. @param iconCodepoint 0 = no icon.
bool MenuItem(const std::string &label, uint32_t iconCodepoint, bool enabled = true);
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
  uint32_t iconCodepoint = 0; ///< 0 = no icon; otherwise drawn as leading glyph in the header.
};

struct TableState {
  int sortColumn = -1;       // -1 = no sort
  bool sortAscending = true;
  float scrollOffset = 0.0f;
  // Phase C7: horizontal scroll offset (only affects non-frozen columns).
  float scrollOffsetX = 0.0f;
  // Phase C7: number of leftmost columns that are pinned (don't horizontal-scroll).
  int frozenColumns = 0;
  // Phase C7: optional multi-row selection set (row indices).
  std::vector<int> selectedRows;
};
/// Sortable, resizable data table. @return true always.
bool BeginTable(const std::string& id, std::vector<TableColumn>& columns,
                int rowCount, const Vec2& size = Vec2(0, 0),
                TableState* state = nullptr);
void TableNextRow();
void TableSetCell(int column);
void EndTable();

/// Phase C7: row selection helper. Call inside a row to make the row
/// click-selectable using the table's selectedRows set (with Ctrl/Shift modifiers).
/// @return true if row was clicked this frame.
bool TableRowSelectable(int rowIndex);

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
