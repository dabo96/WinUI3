#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include "Math/Rect.h"
#include "UI/Layout.h"
#include "UI/Icons.h"
#include "UI/FeedbackWidgets.h" // brief 15: InfoBar, Toast, ProgressRing, Badge, Skeleton
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
bool TextInput(const std::string &label, std::string *value, float width = 200.0f, bool multiline = false, std::optional<Vec2> pos = std::nullopt, const char* placeholder = nullptr, size_t maxLength = 0, bool password = false);

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

// === Flyout & command types (brief 14) ===
// Lightweight command descriptor used by command surfaces (SplitButton,
// DropDownButton, CommandBar, …). NOTE: named CommandItem (not Command) to avoid
// colliding with UndoSystem.h's Command.
struct CommandItem {
    std::string label;
    uint32_t icon = 0;
    std::function<void()> onInvoke;
    bool isPrimary = true;
    bool enabled = true;
};

// Preferred placement of a Flyout relative to its anchor rect. The actual side
// may flip if the preferred side does not fit in the viewport.
enum class FlyoutPlacement {
    Bottom,
    Top,
    Left,
    Right,
    BottomEdgeAlignedLeft,
    BottomEdgeAlignedRight,
    TopEdgeAlignedLeft,
};

// One entry of a MenuFlyout. `submenu` nesting is reserved for a later revision
// (v1 renders top-level entries only).
struct MenuEntry {
    std::string label;
    uint32_t icon = 0;
    std::string accelerator;       ///< Shortcut text shown right-aligned (e.g. "Ctrl+S").
    bool checkable = false;
    bool checked = false;
    bool separator = false;        ///< When true, drawn as a divider; other fields ignored.
    bool enabled = true;
    std::vector<MenuEntry> submenu; ///< Reserved (TODO): nested submenu entries.
    std::function<void()> onInvoke;
};

/// Generic anchored popup with arbitrary content. Open/close by id with
/// OpenFlyout/CloseFlyout. @return true while open (build content between
/// BeginFlyout/EndFlyout). Single-open (like ComboBox); flips and clamps to the
/// viewport. anchorRect is the screen rect of the element it is anchored to.
bool BeginFlyout(const std::string& id, const Rect& anchorRect,
                 FlyoutPlacement placement = FlyoutPlacement::Bottom);
void EndFlyout();
void OpenFlyout(const std::string& id);
void CloseFlyout(const std::string& id);
bool IsFlyoutOpen(const std::string& id);

/// Menu popup built on BeginFlyout: icon/label/accelerator rows, checkable items,
/// separators and keyboard navigation (arrows/Enter/Esc). Submenus are TODO.
/// brief 10 Part F: staggerMs cascades each item's entrance (0 = no stagger). The
/// total delay is capped (~120ms) so long menus still appear promptly.
void MenuFlyout(const std::string& id, const Rect& anchorRect,
                const std::vector<MenuEntry>& entries, float staggerMs = 18.0f);

// === Signature controls (brief 14, sections 1-4, 7-9) ========================

/// On/off pill switch (distinct from Checkbox). Track ~40×20 (DPI-scaled), white
/// circular thumb animated from one end to the other (WidgetState.floatVal at AnimSlot(id,0)
/// 0→1), accent track when ON. Optional on/off status text to the right; the
/// `label` is drawn as a header above. State: `value` or WidgetState.boolVal.
/// Role: CheckBox/Switch. @return true when the value toggles.
bool ToggleSwitch(const std::string& label, bool* value,
                  const std::string& onText = "", const std::string& offText = "",
                  std::optional<Vec2> pos = std::nullopt);

/// Expander: a collapsible card with a header (icon + title + chevron). The body
/// between Begin/End is built only when expanded (call EndExpander only if this
/// returns true). The body height animates/clips open (collapse snaps; brief 10
/// motion degraded). State: `expanded` or WidgetState.boolVal. Role: Group.
bool BeginExpander(const std::string& id, const std::string& header,
                   uint32_t icon = 0, bool* expanded = nullptr);
void EndExpander();

/// SplitButton: primary action zone + dropdown chevron zone (two hit-rects). The
/// chevron opens a MenuFlyout anchored to the button. @return 1 if the primary
/// action was invoked, 2 if the menu was opened, 0 otherwise.
int SplitButton(const std::string& label, uint32_t icon,
                std::function<void()> onPrimary,
                const std::vector<CommandItem>& menu);

/// DropDownButton: a single button that opens a MenuFlyout when pressed.
void DropDownButton(const std::string& label, uint32_t icon,
                    const std::vector<CommandItem>& menu);

/// NumberBox: numeric field with +/- spinners and validation (distinct from
/// DragFloat/SliderInt). Reuses the internal TextInput for editing; parses on
/// Enter/blur, clamps to [min,max], reformats with `format`. Spinners repeat when
/// held; the mouse wheel over the field steps ±step. State buffer in WidgetState.stringVal.
/// Role: Slider/SpinButton. @return true when the value changes.
bool NumberBox(const std::string& label, double* value,
               double min = -1e308, double max = 1e308, double step = 1.0,
               const char* format = "%.0f", std::optional<Vec2> pos = std::nullopt);

/// TeachingTip / coachmark: a popover with a "beak" pointing at targetRect, for
/// onboarding. Built on BeginFlyout. Shows until the user closes it (the "seen"
/// state is persisted by id in WidgetState.boolVal). @return true when the action button
/// (actionText, if non-empty) is pressed.
/// @param open optional one-shot trigger: set *open=true (e.g. from a button) to
///        (re)show the tip on demand; it is consumed back to false. When null, the
///        tip keeps its default auto-show-once behavior.
bool TeachingTip(const std::string& id, const Rect& targetRect,
                 const std::string& title, const std::string& body,
                 const std::string& actionText = "", bool* open = nullptr);

/// Result of a standardized ContentDialog (brief 14 section 9).
enum class DialogResult { None, Primary, Secondary, Close };

/// ContentDialog: a standardized dialog over the existing Modal (scrim + z=Dialog)
/// with title + arbitrary body + standard buttons and a result. Primary button is
/// accented (initial focus); Esc = Close, Enter = Primary; focus is trapped to the
/// dialog's controls. Call each frame while `open`; when it returns != None it
/// sets `*open = false`. Role: Dialog.
DialogResult ContentDialog(const std::string& id, bool* open,
                           const std::string& title,
                           std::function<void()> body,
                           const std::string& primaryText = "OK",
                           const std::string& secondaryText = "",
                           const std::string& closeText = "Cancel");

/// RatingControl: N star glyphs (Lucide) with hover preview, click to set and
/// keyboard arrows. With allowHalf, `*value` counts half-stars (0..2*maxStars),
/// otherwise whole stars (0..maxStars). State: `value` or WidgetState.intVal. Role:
/// Slider; accessibleValue = "X of N". @return true when the value changes.
bool RatingControl(const std::string& id, int* value, int maxStars = 5,
                   bool allowHalf = false);

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

// ─── Layout primitives (brief 19) ────────────────────────────────────────────

/// WrapPanel: lays children out left→right and wraps to a new line when the next
/// item no longer fits the available width (chips, toolbars, simple galleries).
/// Children must be Auto-sized — a Fill child would consume the whole row. The
/// panel reports its total size (full width × accumulated height) to the parent.
/// @param hGap  Horizontal gap between items on a row.
/// @param vGap  Vertical gap between rows.
void BeginWrapPanel(const std::string& id, float hGap = 8.0f, float vGap = 8.0f);
void EndWrapPanel();

/// UniformGrid: N equal-width columns sharing the available width; children fill
/// cells left→right, top→bottom, wrapping every `columns` items. Each cell gets a
/// Fixed-width constraint; cell height is auto (tallest item in the row).
void BeginUniformGrid(const std::string& id, int columns, float gap = 8.0f);
/// Fluid variant: column count is derived from the available width so each cell
/// is at least `minCellWidth` wide (like a responsive GridView).
void BeginUniformGrid(const std::string& id, float minCellWidth, float gap = 8.0f);
/// Advance to the next cell (call between cells, like GridNextCell). The
/// `uniformGrid` UIBuilder sugar calls this automatically between items.
void UniformGridNextCell();
void EndUniformGrid();

/// Responsive breakpoints (logical px, Fluent/WinUI thresholds).
///   Small  < 640 | Medium < 1008 | Large < 1366 | XLarge ≥ 1366
enum class Breakpoint { Small, Medium, Large, XLarge };

/// Breakpoint for a given logical width. `availWidth` <= 0 → use the current
/// container's available width, or the viewport if there is no active container.
/// Physical widths are converted to logical px via the DPI scale.
Breakpoint CurrentBreakpoint(float availWidth = 0.0f);

/// Build `build` only when the current breakpoint is at least `min`.
void VisibleFrom(Breakpoint min, const std::function<void()>& build);

/// Invoke `build` with the current breakpoint so callers can branch their layout.
void AdaptiveLayout(const std::function<void(Breakpoint)>& build);

/// Canvas: an absolute-positioning layer. Children are placed by explicit
/// coordinates (their `pos` param, resolved relative to the canvas origin, or via
/// CanvasChild) instead of the sequential flow cursor; everything is clipped to
/// the canvas rect. Negative coordinates are clamped to 0 by ResolveAbsolutePosition.
void BeginCanvas(const std::string& id, Vec2 size);
/// Position a sub-group at `pos` (relative to the canvas top-left) and build it.
void CanvasChild(Vec2 pos, const std::function<void()>& build);
void EndCanvas();

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

// ═════════════════════════════════════════════════════════════════════════════
// BRIEF 16 — Colecciones a escala de app (GridView, DataGrid, Pagination,
//            ExpanderList, FlipView). Implementación en src/UI/ListWidgets.cpp
//            (GridView/DataGrid/ExpanderList) y src/UI/CollectionWidgets.cpp
//            (Pagination/FlipView).
// ═════════════════════════════════════════════════════════════════════════════

/// GridView: tile/mosaic of items that reflows by available width and virtualizes
/// like ListView (only visible rows are built). Calls @p itemBuilder(index) for
/// each visible item with the cell width pinned via a Fixed constraint.
/// Single-selection + 2D keyboard focus (arrows) when the grid has focus.
/// @param itemSize     Cell size. When @p minItemWidth <= 0, columns are computed
///                     from itemSize.x; cell width stays itemSize.x.
/// @param gap          Spacing between cells (both axes).
/// @param minItemWidth >0 → fluid columns: count derived from width, cells stretch
///                     to share the row (cell width >= minItemWidth).
void GridView(const std::string& id, int itemCount, Vec2 itemSize,
              const std::function<void(int index)>& itemBuilder,
              float gap = 8.0f, float minItemWidth = 0.0f);

/// Column descriptor for DataGrid.
struct DataColumn {
  std::string header;
  float width = 120.0f;
  bool sortable = true;
  bool resizable = true;
  bool editable = false;
  enum class Type { Text, Number, Bool, Choice } type = Type::Text;
  std::vector<std::string> choices; ///< For Type::Choice (fallback editor uses text).
};

/// Result of a DataGrid frame. Indices are in the caller's ORIGINAL (logical)
/// column space, regardless of any visual reordering done by dragging headers.
struct DataGridResult {
  int sortedColumn = -1;   ///< Logical column currently sorted, or -1.
  bool ascending = true;
  int editedRow = -1;      ///< Row whose cell was committed this frame, or -1.
  int editedCol = -1;      ///< Logical column committed this frame, or -1.
};

/// DataGrid: an app-grade editable data grid built on top of the existing Table
/// (frozen columns / sort / resize / row virtualization are reused). Adds column
/// REORDER (drag a header to permute columns; persisted per id) and inline cell
/// EDITING (double-click / Enter on an editable cell; confirm with Enter/blur,
/// cancel with Esc). Editors by type: Text/Number/Choice → TextInput (fallback —
/// NumberBox/ComboBox of brief 14 do not exist yet); Bool → Checkbox.
/// @param getCell  Returns the display string for (row, logicalCol). For Bool,
///                 "true"/"1"/"yes" (case-insensitive) reads as checked.
/// @param setCell  Called with the new value on commit (logical row/col).
DataGridResult DataGrid(const std::string& id, const std::vector<DataColumn>& cols,
                        int rowCount,
                        const std::function<std::string(int row, int col)>& getCell,
                        const std::function<void(int row, int col, const std::string& newVal)>& setCell);

/// Pagination control: ‹ 1 2 … 8 9 … 42 › with collapse of the middle range.
/// Click a page to navigate; Left/Right arrows when focused. Returns the current
/// 0-based page. @p currentPage is optional persistent storage (else per-id state).
int Pagination(const std::string& id, int pageCount, int* currentPage = nullptr);

/// ExpanderList: a vertical list of collapsible headers (built on CollapsingHeader).
/// @param headerFn  Returns the header label for an item.
/// @param bodyFn    Builds the expanded body for an item.
/// @param accordion When true, opening one item closes the others (open index in
///                  WidgetState.intVal); otherwise each item toggles independently.
/// Body height animation (brief 10) degrades to snap (open/closed).
void ExpanderList(const std::string& id, int itemCount,
                  const std::function<std::string(int)>& headerFn,
                  const std::function<void(int)>& bodyFn,
                  bool accordion = false);

/// FlipView / Carousel: one item visible at a time with ‹ › arrows and dot
/// indicators. Returns the current index. @p currentIndex is optional persistent
/// storage (else per-id state). Navigation wraps. Slide transition (brief 10) is a
/// lightweight slide-in; degrades gracefully to a cut when motion is unavailable.
int FlipView(const std::string& id, int itemCount,
             const std::function<void(int index)>& itemBuilder, int* currentIndex = nullptr);

/// @}

// ═════════════════════════════════════════════════════════════════════════════
// BRIEF 17 — Texto y contenido rico (selección/copia, hyperlinks, AutoSuggest,
//            chips/tokens, password, Markdown). SelectableText / PasswordBox /
//            AutoSuggestBox / TokenizingTextBox en src/UI/InputWidgets.cpp;
//            HyperlinkButton en src/UI/BasicWidgets.cpp; MarkdownView en
//            src/UI/MarkdownWidgets.cpp.
// ═════════════════════════════════════════════════════════════════════════════

/// SelectableText: read-only text the user can select and copy. Selection range
/// (anchor + caret, byte offsets) is kept by @p id in WidgetState.intVal. Mouse: down sets
/// anchor, drag moves caret, double-click selects a word, triple-click a line.
/// Keyboard when focused: Shift+Left/Right extend, Ctrl+A all, Ctrl+C / Ctrl+Insert
/// copy to the OS clipboard (brief 18). Highlight (translucent accent) is drawn
/// before the glyphs. @param fontSize 0 = Body. @param wrap word-wrap to the
/// available width. Role: Label.
void SelectableText(const std::string& id, const std::string& text,
                    float fontSize = 0, bool wrap = true,
                    std::optional<Vec2> pos = std::nullopt);

/// HyperlinkButton: accent-colored text, underlined on hover, hand cursor. Click
/// (or Enter when focused) opens @p url with the OS (the OS URL opener) when non-empty.
/// Usable inline beside Label in a horizontal row. @param fontSize 0 = Body.
/// @return true when activated this frame.
bool HyperlinkButton(const std::string& text, const std::string& url = "",
                     float fontSize = 0);

/// AutoSuggestBox: search field with a popup of suggestions (TextInput + Flyout).
/// @p suggestionsFn receives the current text and returns the list to show; the
/// matched substring is highlighted. Up/Down move the highlight, Enter picks it,
/// Esc/click-outside close, click picks. @return the chosen suggestion this frame
/// (empty if none). @p text is the live edit buffer (or per-id state when null).
std::string AutoSuggestBox(const std::string& id, std::string* text,
                           const std::function<std::vector<std::string>(const std::string&)>& suggestionsFn,
                           const std::string& placeholder = "");

/// TokenizingTextBox / Chips: multi-value entry. Existing tokens render as pills
/// (text + "x") that wrap (WrapPanel, brief 19), followed by an inline text field.
/// Enter / comma confirm a token; Backspace on an empty field deletes the last
/// chip; clicking "x" deletes that chip. Optional @p suggestionsFn shows a popup.
/// @return true when @p tokens changes this frame.
bool TokenizingTextBox(const std::string& id, std::vector<std::string>* tokens,
                       const std::string& placeholder = "",
                       const std::function<std::vector<std::string>(const std::string&)>& suggestionsFn = {});

/// PasswordBox: single-line masked field. Like TextInput but draws '•' per
/// codepoint, with an eye button to reveal/hide. Copy/Cut are disabled so the
/// secret cannot leave via the clipboard (paste is allowed). Claims IME per-field
/// on focus (brief 18). @return true when the value changes.
bool PasswordBox(const std::string& id, std::string* value,
                 const std::string& placeholder = "",
                 std::optional<Vec2> pos = std::nullopt,
                 float width = 300.0f);

/// MarkdownView: render a read-only subset of Markdown — headings (#..###),
/// **bold** / *italic* / `code`, unordered lists (- / *), block quotes (>), links
/// [txt](url) (via HyperlinkButton), images ![alt](url) (DrawImage if a texture is
/// registered for the url, else a placeholder), and horizontal rules (---). Inline
/// emphasis is rendered segment-by-segment with weight/font; paragraphs wrap by
/// word. @param maxWidth 0 = current layout width.
void MarkdownView(const std::string& id, const std::string& markdown,
                  float maxWidth = 0);

/// Register a texture handle for a Markdown image url (used by MarkdownView's
/// ![alt](url)). Backend-specific handle (e.g. GLuint cast to void*); pass with an
/// intrinsic pixel size for layout. Pass nullptr to unregister.
void MarkdownRegisterImage(const std::string& url, void* textureHandle, Vec2 size);

} // namespace FluentUI

// brief 13: App shell & navegación (NavigationView, NavFrame, CommandBar,
// BreadcrumbBar, TitleBar). Incluido FUERA del namespace (NavigationWidgets.h
// abre su propio `namespace FluentUI`); va tras la definición completa de
// CommandItem/MenuEntry para que CommandBar pueda usar std::vector<CommandItem>.
#include "UI/NavigationWidgets.h"
