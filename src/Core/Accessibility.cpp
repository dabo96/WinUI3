#include "core/Accessibility.h"
#include "core/WidgetTree.h"

// brief 18.3: Windows UI Automation COM headers MUST be included at global scope
// (outside any namespace) so Win32/UIA symbols land where they belong.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <uiautomation.h>
#if defined(_MSC_VER)
#pragma comment(lib, "uiautomationcore.lib")
#pragma comment(lib, "oleaut32.lib")
#endif
#endif

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
// =============================================================================
// brief 18.3 — Windows UI Automation provider.
//
// Functional minimum: a real COM IRawElementProviderSimple server is attached to
// the window via WM_GETOBJECT (the window is subclassed so we can return an
// LRESULT, which SDL's message hook cannot). The root element delegates its
// bounding box / window semantics to the HWND host provider, exposes a Name and
// a Pane control type, and is reference-counted correctly. Narrator/NVDA detect a
// FluentUI automation peer on the window.
//
// TODO (next pass, not blocking): expose the *per-widget* fragment tree. The data
// is ready (GetAccessibleWidgets → role/name/value/bounds/focused). The work is to
// implement IRawElementProviderFragment + IRawElementProviderFragmentRoot, map
// AccessibleRole→UIA_*ControlTypeId, return BoundingRectangle from node->bounds,
// Navigate() over the widget tree, and SetFocus/raise events per node. Live
// regions (Toast/InfoBar, brief 15) and dialog focus-trap (ContentDialog, brief
// 14) raise UiaRaiseAutomationEvent / structure-changed once fragments exist.
// Multi-window: this single-provider hook assumes one main window; per-HWND
// providers are a follow-up.
// =============================================================================

namespace {

// Minimal server-side root provider. Implements IRawElementProviderSimple and
// defers host/window semantics (bounds, native window pattern) to the HWND.
class FluentRootProvider : public IRawElementProviderSimple {
public:
    explicit FluentRootProvider(HWND hwnd) : refCount_(1), hwnd_(hwnd) {}
    virtual ~FluentRootProvider() = default;

    // --- IUnknown ---
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) ||
            riid == __uuidof(IRawElementProviderSimple)) {
            *ppv = static_cast<IRawElementProviderSimple*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return static_cast<ULONG>(c);
    }

    // --- IRawElementProviderSimple ---
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* opts) override {
        if (!opts) return E_POINTER;
        *opts = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider |
                                             ProviderOptions_UseComThreading);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown** ret) override {
        if (!ret) return E_POINTER;
        *ret = nullptr; // no control patterns on the root yet (TODO: per-widget)
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId,
                                               VARIANT* ret) override {
        if (!ret) return E_POINTER;
        VariantInit(ret);
        if (propertyId == UIA_NamePropertyId) {
            ret->vt = VT_BSTR;
            ret->bstrVal = SysAllocString(L"FluentUI");
            return S_OK;
        }
        if (propertyId == UIA_ControlTypePropertyId) {
            ret->vt = VT_I4;
            ret->lVal = UIA_PaneControlTypeId;
            return S_OK;
        }
        ret->vt = VT_EMPTY;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
            IRawElementProviderSimple** ret) override {
        if (!ret) return E_POINTER;
        // The HWND host provider supplies the bounding rectangle and the native
        // window pattern, so screen readers get a valid element for the window.
        return UiaHostProviderFromHwnd(hwnd_, ret);
    }

private:
    LONG refCount_;
    HWND hwnd_;
};

HWND               g_uiaHwnd      = nullptr;
UIContext*         g_uiaCtx       = nullptr;
FluentRootProvider* g_uiaProvider = nullptr;
WNDPROC            g_uiaPrevProc  = nullptr;

LRESULT CALLBACK UiaWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_GETOBJECT &&
        static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId)) {
        if (g_uiaProvider) {
            return UiaReturnRawElementProvider(
                hwnd, wParam, lParam,
                static_cast<IRawElementProviderSimple*>(g_uiaProvider));
        }
    }
    if (msg == WM_DESTROY && g_uiaHwnd == hwnd) {
        // Disconnect providers before the window goes away.
        UiaDisconnectAllProviders();
    }
    WNDPROC prev = g_uiaPrevProc;
    if (prev) return CallWindowProcW(prev, hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool InitUIAutomation(void* hwnd, UIContext* ctx) {
    if (!hwnd) {
        Log(LogLevel::Warning, "UIAutomation: null HWND");
        return false;
    }
    if (g_uiaHwnd) {
        Log(LogLevel::Warning, "UIAutomation: already initialized");
        return false;
    }
    g_uiaHwnd = static_cast<HWND>(hwnd);
    g_uiaCtx = ctx;
    g_uiaProvider = new FluentRootProvider(g_uiaHwnd);

    // Subclass the window so WM_GETOBJECT can return our provider as an LRESULT.
    g_uiaPrevProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_uiaHwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(&UiaWndProc)));
    if (!g_uiaPrevProc) {
        Log(LogLevel::Warning, "UIAutomation: failed to subclass window");
        g_uiaProvider->Release();
        g_uiaProvider = nullptr;
        g_uiaHwnd = nullptr;
        g_uiaCtx = nullptr;
        return false;
    }
    Log(LogLevel::Info, "UIAutomation: root provider attached (per-widget fragments TODO)");
    return true;
}

void ShutdownUIAutomation() {
    if (g_uiaHwnd && g_uiaPrevProc) {
        // Restore the original window procedure.
        SetWindowLongPtrW(g_uiaHwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_uiaPrevProc));
    }
    UiaDisconnectAllProviders();
    if (g_uiaProvider) {
        g_uiaProvider->Release();
        g_uiaProvider = nullptr;
    }
    g_uiaPrevProc = nullptr;
    g_uiaHwnd = nullptr;
    g_uiaCtx = nullptr;
    Log(LogLevel::Info, "UIAutomation: Provider shut down");
}
#endif

} // namespace FluentUI
