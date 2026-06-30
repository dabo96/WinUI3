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

    // ─── brief 10 Part D — presence tracking (enter / exit transitions) ──────────
    // A managed overlay (Flyout/Menu/Tooltip/Modal/Dialog/Toast) marks its node with
    // animatesExit=true via BeginPresence(). While present the node is re-emitted each
    // frame; when it stops being emitted the presence driver (Context.cpp, between
    // frame build and Reconcile) flips it to Exiting and animates opacity→0 + scale→
    // ~0.96 before letting Reconcile remove it. enterT drives fade+scale on entrance.
    enum class Presence : uint8_t { Entering, Present, Exiting };
    Presence presence = Presence::Entering;
    AnimatedValue<float> enterT{0.0f};  // 0→1 on appear (fade/scale factor)
    float scaleAnim = 1.0f;             // current scale (1=full); driven on exit
    bool animatesExit = false;          // opted-in by BeginPresence for managed overlays

    // ─── brief 10 Part E — FLIP layout animation ────────────────────────────────
    // Opt-in (animateLayout containers). prevBounds is last frame's rect; when a
    // child's new bounds differ, posSpring is seeded with the delta and decays to 0,
    // so the child slides from its old position to the new one. offset = posSpring.Get().
    Rect prevBounds;
    bool prevBoundsValid = false;
    SpringValue<Vec2> posSpring;        // visual offset (delta→0)
    bool animatesLayout = false;        // set by FLIP-enabled containers

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

    // brief 10 Part D/E/G: true while a presence (enter/exit) or FLIP spring is
    // running. Distinct from hasActiveAnimations (which gates bg/opacity tweens) so
    // the presence/FLIP drivers can advance these even on nodes that don't set it.
    bool IsMotionActive() const {
        return enterT.IsAnimating() || posSpring.IsAnimating() ||
               presence == Presence::Exiting;
    }

    // Virtual interface — subclasses override as needed
    virtual void Update(float dt) {
        // brief 10 Part D/E: advance presence + FLIP motion independently of the
        // hasActiveAnimations fast-path (which only covers bg/opacity tweens).
        enterT.Update(dt);
        posSpring.Update(dt);

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
