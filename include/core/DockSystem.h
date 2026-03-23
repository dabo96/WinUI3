#pragma once
#include "Math/Vec2.h"
#include "Math/Rect.h"
#include "Math/Color.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace FluentUI {

// Forward declarations
struct UIContext;
class UIBuilder;
class Renderer;

// Dock position for placing panels
enum class DockPosition {
    Left,
    Right,
    Top,
    Bottom,
    Center,   // Tab group
    Float     // Floating (undocked)
};

// A node in the dock tree — either a split, a tab group, or a single leaf
class DockNode {
public:
    enum class Type { Split, Tab, Leaf, Empty };

    Type type = Type::Empty;

    // Split properties
    bool splitVertical = true;   // true = vertical divider (left|right)
    float splitRatio = 0.5f;
    std::unique_ptr<DockNode> first;
    std::unique_ptr<DockNode> second;

    // Tab/Leaf properties
    std::vector<std::string> panelIds;   // For Tab: multiple panel IDs
    std::string panelId;                  // For Leaf: single panel ID
    int activeTabIndex = 0;

    // Layout (computed each frame)
    Rect bounds;

    // Interaction state
    bool resizing = false;
    float resizeStartMouse = 0.0f;
    float resizeStartRatio = 0.0f;

    DockNode() = default;

    // Factory helpers
    static std::unique_ptr<DockNode> MakeLeaf(const std::string& id) {
        auto node = std::make_unique<DockNode>();
        node->type = Type::Leaf;
        node->panelId = id;
        return node;
    }

    static std::unique_ptr<DockNode> MakeTab(const std::vector<std::string>& ids) {
        auto node = std::make_unique<DockNode>();
        node->type = Type::Tab;
        node->panelIds = ids;
        return node;
    }

    static std::unique_ptr<DockNode> MakeSplit(bool vertical, float ratio,
                                                std::unique_ptr<DockNode> a,
                                                std::unique_ptr<DockNode> b) {
        auto node = std::make_unique<DockNode>();
        node->type = Type::Split;
        node->splitVertical = vertical;
        node->splitRatio = ratio;
        node->first = std::move(a);
        node->second = std::move(b);
        return node;
    }

    // Find the leaf node containing a panel ID
    DockNode* FindPanel(const std::string& id) {
        if (type == Type::Leaf && panelId == id) return this;
        if (type == Type::Tab) {
            for (auto& pid : panelIds) {
                if (pid == id) return this;
            }
        }
        if (first) {
            if (auto* found = first->FindPanel(id)) return found;
        }
        if (second) {
            if (auto* found = second->FindPanel(id)) return found;
        }
        return nullptr;
    }

    // Remove a panel from the tree, returns true if node should be removed
    bool RemovePanel(const std::string& id) {
        if (type == Type::Leaf && panelId == id) {
            type = Type::Empty;
            panelId.clear();
            return true;
        }
        if (type == Type::Tab) {
            auto it = std::find(panelIds.begin(), panelIds.end(), id);
            if (it != panelIds.end()) {
                panelIds.erase(it);
                if (activeTabIndex >= static_cast<int>(panelIds.size()))
                    activeTabIndex = std::max(0, static_cast<int>(panelIds.size()) - 1);
                if (panelIds.empty()) {
                    type = Type::Empty;
                    return true;
                }
                return false;
            }
        }
        if (type == Type::Split) {
            bool firstEmpty = first && first->RemovePanel(id);
            bool secondEmpty = second && second->RemovePanel(id);
            // If one child becomes empty, promote the other
            if (firstEmpty && second) {
                auto promoted = std::move(second);
                type = promoted->type;
                splitVertical = promoted->splitVertical;
                splitRatio = promoted->splitRatio;
                panelId = std::move(promoted->panelId);
                panelIds = std::move(promoted->panelIds);
                activeTabIndex = promoted->activeTabIndex;
                first = std::move(promoted->first);
                second = std::move(promoted->second);
                return false;
            }
            if (secondEmpty && first) {
                auto promoted = std::move(first);
                type = promoted->type;
                splitVertical = promoted->splitVertical;
                splitRatio = promoted->splitRatio;
                panelId = std::move(promoted->panelId);
                panelIds = std::move(promoted->panelIds);
                activeTabIndex = promoted->activeTabIndex;
                first = std::move(promoted->first);
                second = std::move(promoted->second);
                return false;
            }
            if (firstEmpty && secondEmpty) {
                type = Type::Empty;
                first.reset();
                second.reset();
                return true;
            }
        }
        return false;
    }

    // Collect all panel IDs in this subtree
    void CollectPanelIds(std::vector<std::string>& out) const {
        if (type == Type::Leaf && !panelId.empty()) {
            out.push_back(panelId);
        }
        if (type == Type::Tab) {
            for (auto& id : panelIds) out.push_back(id);
        }
        if (first) first->CollectPanelIds(out);
        if (second) second->CollectPanelIds(out);
    }
};

// Manages the dock layout for a window
class DockSpace {
public:
    DockSpace() = default;

    // Dock a panel at a position relative to the entire dock space
    void DockPanel(const std::string& panelId, DockPosition pos,
                   const std::string& relativeTo = "") {
        // Remove if already docked somewhere
        if (root_) root_->RemovePanel(panelId);

        if (!root_ || root_->type == DockNode::Type::Empty) {
            root_ = DockNode::MakeLeaf(panelId);
            return;
        }

        // Dock relative to another panel or to the root
        DockNode* target = nullptr;
        if (!relativeTo.empty() && root_) {
            target = root_->FindPanel(relativeTo);
        }

        if (pos == DockPosition::Center) {
            if (target) {
                // Explicit target: create a tab group with that panel
                if (target->type == DockNode::Type::Leaf) {
                    std::string existingId = target->panelId;
                    target->type = DockNode::Type::Tab;
                    target->panelIds = {existingId, panelId};
                    target->panelId.clear();
                    target->activeTabIndex = 0;
                } else if (target->type == DockNode::Type::Tab) {
                    target->panelIds.push_back(panelId);
                }
            } else {
                // No target: fill the remaining space
                // The existing root keeps its size, new panel fills the rest (75%)
                auto oldRoot = std::move(root_);
                root_ = DockNode::MakeSplit(true, 0.25f,
                                            std::move(oldRoot),
                                            DockNode::MakeLeaf(panelId));
            }
            return;
        }

        // Directional dock: create a split
        bool vertical = (pos == DockPosition::Left || pos == DockPosition::Right);
        bool newFirst = (pos == DockPosition::Left || pos == DockPosition::Top);
        float ratio = 0.25f; // New panel takes 25% by default

        auto newLeaf = DockNode::MakeLeaf(panelId);

        if (target && target != root_.get()) {
            // Dock relative to a specific panel — need to find parent and replace
            // For simplicity, dock relative to root when target is nested
            auto oldRoot = std::move(root_);
            if (newFirst) {
                root_ = DockNode::MakeSplit(vertical, ratio, std::move(newLeaf), std::move(oldRoot));
            } else {
                root_ = DockNode::MakeSplit(vertical, 1.0f - ratio, std::move(oldRoot), std::move(newLeaf));
            }
        } else {
            auto oldRoot = std::move(root_);
            if (newFirst) {
                root_ = DockNode::MakeSplit(vertical, ratio, std::move(newLeaf), std::move(oldRoot));
            } else {
                root_ = DockNode::MakeSplit(vertical, 1.0f - ratio, std::move(oldRoot), std::move(newLeaf));
            }
        }
    }

    // Undock a panel (remove from dock tree)
    void UndockPanel(const std::string& panelId) {
        if (root_) {
            root_->RemovePanel(panelId);
            if (root_->type == DockNode::Type::Empty) {
                root_.reset();
            }
        }
    }

    // Check if a panel is docked
    bool IsPanelDocked(const std::string& panelId) const {
        if (!root_) return false;
        return const_cast<DockNode*>(root_.get())->FindPanel(panelId) != nullptr;
    }

    // Compute layout bounds for all nodes recursively
    void ComputeLayout(const Rect& availableArea) {
        availableArea_ = availableArea;
        if (root_) {
            ComputeNodeLayout(root_.get(), availableArea);
        }
    }

    // Get the bounds for a specific panel
    Rect GetPanelBounds(const std::string& panelId) const {
        if (!root_) return {};
        auto* node = const_cast<DockNode*>(root_.get())->FindPanel(panelId);
        return node ? node->bounds : Rect{};
    }

    // Get all docked panel IDs
    std::vector<std::string> GetDockedPanels() const {
        std::vector<std::string> result;
        if (root_) root_->CollectPanelIds(result);
        return result;
    }

    // Access root node (for rendering/interaction)
    DockNode* Root() { return root_.get(); }
    const DockNode* Root() const { return root_.get(); }

    // Check if dock space has any content
    bool IsEmpty() const { return !root_ || root_->type == DockNode::Type::Empty; }

    // Set root node directly (for deserialization)
    void SetRoot(std::unique_ptr<DockNode> root) { root_ = std::move(root); }

    // Handle split resize interaction
    void HandleInteraction(float mouseX, float mouseY, bool mouseDown, bool mousePressed);

    // Get available area
    const Rect& GetAvailableArea() const { return availableArea_; }

    // Divider thickness for splits
    static constexpr float DIVIDER_THICKNESS = 6.0f;

private:
    void ComputeNodeLayout(DockNode* node, const Rect& area) {
        node->bounds = area;

        if (node->type == DockNode::Type::Split && node->first && node->second) {
            float divider = DIVIDER_THICKNESS;
            if (node->splitVertical) {
                // Left | Right
                float firstW = (area.size.x - divider) * node->splitRatio;
                float secondW = area.size.x - divider - firstW;
                ComputeNodeLayout(node->first.get(),
                    Rect{area.pos, {firstW, area.size.y}});
                ComputeNodeLayout(node->second.get(),
                    Rect{{area.pos.x + firstW + divider, area.pos.y}, {secondW, area.size.y}});
            } else {
                // Top / Bottom
                float firstH = (area.size.y - divider) * node->splitRatio;
                float secondH = area.size.y - divider - firstH;
                ComputeNodeLayout(node->first.get(),
                    Rect{area.pos, {area.size.x, firstH}});
                ComputeNodeLayout(node->second.get(),
                    Rect{{area.pos.x, area.pos.y + firstH + divider}, {area.size.x, secondH}});
            }
        }
    }

    std::unique_ptr<DockNode> root_;
    Rect availableArea_;
};

/// Determine which dock zone the mouse is over for a given panel bounds.
/// Returns DockPosition::Float if not over any zone.
DockPosition HitTestDockZones(const Rect& panelBounds, float mouseX, float mouseY);

/// Get the preview rect for where a panel would be placed in a given zone.
Rect GetDockZonePreviewRect(const Rect& targetBounds, DockPosition zone);

} // namespace FluentUI
