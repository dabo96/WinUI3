#include "core/Accessibility.h"
#include "core/WidgetTree.h"

namespace FluentUI {

static AccessibilityCallback g_a11yCallback = nullptr;

void SetAccessibilityCallback(AccessibilityCallback callback) {
    g_a11yCallback = std::move(callback);
}

void FireAccessibilityEvent(AccessibilityEvent event, const WidgetNode* node) {
    if (g_a11yCallback) {
        g_a11yCallback(event, node);
    }
}

std::vector<const WidgetNode*> GetAccessibleWidgets(const UIContext* ctx) {
    std::vector<const WidgetNode*> result;
    if (!ctx) return result;

    // TraverseDepthFirst is non-const — safe to cast away const for read-only traversal
    auto& tree = const_cast<WidgetTree&>(ctx->widgetTree);
    tree.TraverseDepthFirst([&](WidgetNode* node) {
        if (node->accessibleRole != WidgetNode::AccessibleRole::None &&
            node->visible && node->alive) {
            result.push_back(node);
        }
    });
    return result;
}

const char* AccessibleRoleToString(WidgetNode::AccessibleRole role) {
    switch (role) {
    case WidgetNode::AccessibleRole::None:        return "None";
    case WidgetNode::AccessibleRole::Button:      return "Button";
    case WidgetNode::AccessibleRole::CheckBox:    return "CheckBox";
    case WidgetNode::AccessibleRole::RadioButton: return "RadioButton";
    case WidgetNode::AccessibleRole::Slider:      return "Slider";
    case WidgetNode::AccessibleRole::TextInput:   return "TextInput";
    case WidgetNode::AccessibleRole::ComboBox:    return "ComboBox";
    case WidgetNode::AccessibleRole::ListItem:    return "ListItem";
    case WidgetNode::AccessibleRole::TreeItem:    return "TreeItem";
    case WidgetNode::AccessibleRole::MenuItem:    return "MenuItem";
    case WidgetNode::AccessibleRole::Tab:         return "Tab";
    case WidgetNode::AccessibleRole::Panel:       return "Panel";
    case WidgetNode::AccessibleRole::ScrollBar:   return "ScrollBar";
    case WidgetNode::AccessibleRole::ProgressBar: return "ProgressBar";
    case WidgetNode::AccessibleRole::Dialog:      return "Dialog";
    case WidgetNode::AccessibleRole::Table:       return "Table";
    case WidgetNode::AccessibleRole::Image:       return "Image";
    case WidgetNode::AccessibleRole::Label:       return "Label";
    case WidgetNode::AccessibleRole::Group:       return "Group";
    default: return "Unknown";
    }
}

#ifdef _WIN32
// Minimal UIA provider stub — full implementation requires COM interop
// with UIAutomationCore.h. This provides the framework hooks;
// a complete provider would implement IRawElementProviderSimple per node.

static void* g_uiaHwnd = nullptr;
static UIContext* g_uiaCtx = nullptr;

bool InitUIAutomation(void* hwnd, UIContext* ctx) {
    g_uiaHwnd = hwnd;
    g_uiaCtx = ctx;
    Log(LogLevel::Info, "UIAutomation: Provider initialized (stub)");
    return true;
}

void ShutdownUIAutomation() {
    g_uiaHwnd = nullptr;
    g_uiaCtx = nullptr;
    Log(LogLevel::Info, "UIAutomation: Provider shut down");
}
#endif

} // namespace FluentUI
