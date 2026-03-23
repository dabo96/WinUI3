#pragma once
#include "core/WidgetNode.h"
#include "core/Context.h"
#include <string>
#include <vector>

namespace FluentUI {

/// Accessibility event types fired when widget state changes.
enum class AccessibilityEvent {
    FocusChanged,       ///< A widget gained focus
    ValueChanged,       ///< A widget's value changed (slider, text, checkbox)
    StructureChanged,   ///< The widget tree structure changed
    SelectionChanged,   ///< List/tree selection changed
    ExpandedChanged,    ///< A tree node or combo expanded/collapsed
    NameChanged         ///< A widget's accessible name changed
};

/// Callback type for accessibility events.
using AccessibilityCallback = std::function<void(AccessibilityEvent, const WidgetNode*)>;

/// Set a callback to receive accessibility events (e.g. for UIA bridge).
void SetAccessibilityCallback(AccessibilityCallback callback);

/// Fire an accessibility event (call from widget code when state changes).
void FireAccessibilityEvent(AccessibilityEvent event, const WidgetNode* node);

/// Collect a flat list of all focusable widgets in tree order.
/// Useful for screen readers that enumerate controls.
std::vector<const WidgetNode*> GetAccessibleWidgets(const UIContext* ctx);

/// Get the accessible name for a node (falls back to debugName).
inline const std::string& GetAccessibleName(const WidgetNode* node) {
    if (!node) {
        static const std::string empty;
        return empty;
    }
    return node->accessibleName.empty() ? node->debugName : node->accessibleName;
}

/// Convert AccessibleRole to a human-readable string (for debug/UIA).
const char* AccessibleRoleToString(WidgetNode::AccessibleRole role);

#ifdef _WIN32
/// Initialize Windows UI Automation provider for the given window.
/// Call once after window creation. Returns true on success.
/// This registers a minimal IRawElementProviderSimple that exposes
/// the FluentUI widget tree to screen readers like Narrator and NVDA.
bool InitUIAutomation(void* hwnd, UIContext* ctx);

/// Shut down the UIA provider. Call before window destruction.
void ShutdownUIAutomation();
#endif

} // namespace FluentUI
