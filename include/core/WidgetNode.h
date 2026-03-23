#pragma once
#include "Math/Vec2.h"
#include "Math/Rect.h"
#include "Math/Color.h"
#include "core/Animation.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace FluentUI {

// Forward declarations
class Renderer;
class InputState;
struct Style;

// Node type identifiers for safe downcasting without RTTI
enum class WidgetNodeType : uint8_t {
    Generic = 0,
    Panel,
    ScrollView,
    TabView,
    ListView,
    TreeView,
    Modal,
    MenuBar,
    ContextMenu,
    Splitter,
    DockSpace,
    Table
};

class WidgetNode {
public:
    // Identity
    uint32_t id = 0;
    std::string debugName;
    WidgetNodeType nodeType = WidgetNodeType::Generic;

    // Tree structure
    WidgetNode* parent = nullptr;
    std::vector<std::unique_ptr<WidgetNode>> children;

    // Lifecycle
    uint32_t lastFrameSeen = 0;
    bool alive = true;

    // Layout (cached from last frame)
    Rect bounds;
    Rect clipRect;
    bool visible = true;

    // Interaction state (inline, avoids map lookups)
    bool hovered = false;
    bool pressed = false;
    bool focused = false;

    // Accessibility properties
    enum class AccessibleRole : uint8_t {
        None = 0, Button, CheckBox, RadioButton, Slider, TextInput,
        ComboBox, ListItem, TreeItem, MenuItem, Tab, Panel,
        ScrollBar, ProgressBar, Dialog, Table, Image, Label, Group
    };
    AccessibleRole accessibleRole = AccessibleRole::None;
    std::string accessibleName;   ///< Screen-reader label (defaults to debugName)
    std::string accessibleValue;  ///< Current value (e.g. slider %, checkbox state)
    bool accessibleExpanded = false;  ///< For tree nodes, combo boxes

    // Inline animations (avoids global animation maps)
    AnimatedValue<Color> bgColorAnim;
    AnimatedValue<float> opacityAnim{1.0f};

    // Style override (nullptr = inherit from theme)
    Style* styleOverride = nullptr;

    // Construction / Destruction
    WidgetNode() = default;
    explicit WidgetNode(uint32_t nodeId, WidgetNodeType type = WidgetNodeType::Generic)
        : id(nodeId), nodeType(type) {}
    virtual ~WidgetNode() = default;

    // Non-copyable, movable
    WidgetNode(const WidgetNode&) = delete;
    WidgetNode& operator=(const WidgetNode&) = delete;
    WidgetNode(WidgetNode&&) = default;
    WidgetNode& operator=(WidgetNode&&) = default;

    // Perf 2.3: Flag to skip nodes without active animations
    bool hasActiveAnimations = false;

    // Virtual interface — subclasses override as needed
    virtual void Update(float dt) {
        if (!hasActiveAnimations) return;
        bgColorAnim.Update(dt);
        opacityAnim.Update(dt);
        // Clear flag if no animations are active anymore
        if (!bgColorAnim.IsAnimating() && !opacityAnim.IsAnimating()) {
            hasActiveAnimations = false;
        }
    }

    virtual void Draw(Renderer& /*renderer*/) {}
    virtual void HandleInput(InputState& /*input*/) {}
    virtual Vec2 ComputeDesiredSize() { return bounds.size; }

    // --- Tree operations ---

    // Find immediate child by ID (linear scan, children count is typically small)
    WidgetNode* FindChild(uint32_t childId) {
        for (auto& child : children) {
            if (child && child->id == childId) return child.get();
        }
        return nullptr;
    }

    // Find descendant by ID (depth-first)
    WidgetNode* FindDescendant(uint32_t targetId) {
        if (id == targetId) return this;
        for (auto& child : children) {
            if (auto* found = child->FindDescendant(targetId)) return found;
        }
        return nullptr;
    }

    // Remove children not seen in the current frame
    // Perf 1.6: Returns true if any nodes were actually removed
    bool RemoveDeadChildren(uint32_t currentFrame, uint32_t gracePeriod = 1) {
        size_t sizeBefore = children.size();
        children.erase(
            std::remove_if(children.begin(), children.end(),
                [currentFrame, gracePeriod](const std::unique_ptr<WidgetNode>& child) {
                    return child && (currentFrame - child->lastFrameSeen) > gracePeriod;
                }),
            children.end()
        );
        bool removed = (children.size() != sizeBefore);
        // Recurse
        for (auto& child : children) {
            if (child->RemoveDeadChildren(currentFrame, gracePeriod))
                removed = true;
        }
        return removed;
    }

    // Mark this node (and optionally children) as seen this frame
    void MarkSeen(uint32_t frame) {
        lastFrameSeen = frame;
        alive = true;
    }

    // Traverse depth-first
    void TraverseDepthFirst(const std::function<void(WidgetNode*)>& visitor) {
        visitor(this);
        for (auto& child : children) {
            child->TraverseDepthFirst(visitor);
        }
    }

    // Add a child node, returns raw pointer
    WidgetNode* AddChild(std::unique_ptr<WidgetNode> child) {
        child->parent = this;
        children.push_back(std::move(child));
        return children.back().get();
    }

    // Get child count
    size_t ChildCount() const { return children.size(); }
};

} // namespace FluentUI
