#pragma once
#include "core/WidgetNode.h"
#include <functional>
#include <unordered_map>
#include <cassert>

namespace FluentUI {

class WidgetTree {
public:
    WidgetTree() {
        root = std::make_unique<WidgetNode>(0, WidgetNodeType::Generic);
        root->debugName = "root";
        currentParent_ = root.get();
    }

    // --- Core operations ---

    // Find existing child of currentParent by ID, or create with factory.
    // Marks the node as seen this frame.
    WidgetNode* FindOrCreate(uint32_t id, uint32_t frame,
                             std::function<std::unique_ptr<WidgetNode>()> factory) {
        WidgetNode* existing = currentParent_->FindChild(id);
        if (existing) {
            existing->MarkSeen(frame);
            return existing;
        }
        // Create new node
        auto node = factory();
        node->id = id;
        node->MarkSeen(frame);
        WidgetNode* ptr = currentParent_->AddChild(std::move(node));
        // Cache in lookup table
        idLookup_[id] = ptr;
        return ptr;
    }

    // Push a node as the current parent (for Begin* calls)
    void PushParent(WidgetNode* node) {
        parentStack_.push_back(currentParent_);
        currentParent_ = node;
    }

    // Pop back to the previous parent (for End* calls)
    void PopParent() {
        if (!parentStack_.empty()) {
            currentParent_ = parentStack_.back();
            parentStack_.pop_back();
        }
    }

    // Current parent (where new widgets will be inserted)
    WidgetNode* CurrentParent() const { return currentParent_; }

    // --- Reconciliation ---

    // Remove nodes that haven't been seen for gracePeriod frames.
    // Call once per frame in NewFrame().
    // Perf 1.6: Only rebuild lookup if nodes were actually removed
    void Reconcile(uint32_t currentFrame, uint32_t gracePeriod = 2) {
        bool anyRemoved = root->RemoveDeadChildren(currentFrame, gracePeriod);
        if (anyRemoved) {
            RebuildLookup();
        }
    }

    // --- Queries ---

    // Fast ID-based lookup (O(1) average)
    WidgetNode* FindById(uint32_t id) {
        auto it = idLookup_.find(id);
        return (it != idLookup_.end()) ? it->second : nullptr;
    }

    // Traverse entire tree depth-first
    void TraverseDepthFirst(const std::function<void(WidgetNode*)>& visitor) {
        if (root) root->TraverseDepthFirst(visitor);
    }

    // Perf 2.3: Only update nodes with active animations
    void UpdateAnimations(float dt) {
        TraverseDepthFirst([dt](WidgetNode* node) {
            if (node->hasActiveAnimations) {
                node->Update(dt);
            }
        });
    }

    // Get root node
    WidgetNode* Root() { return root.get(); }
    const WidgetNode* Root() const { return root.get(); }

    // Node count (for debugging)
    size_t NodeCount() const { return idLookup_.size(); }

    // Reset parent stack to root (call at start of frame)
    void ResetParentStack() {
        parentStack_.clear();
        currentParent_ = root.get();
    }

private:
    void RebuildLookup() {
        idLookup_.clear();
        TraverseDepthFirst([this](WidgetNode* node) {
            if (node->id != 0) { // Skip root
                idLookup_[node->id] = node;
            }
        });
    }

    std::unique_ptr<WidgetNode> root;
    WidgetNode* currentParent_ = nullptr;
    std::vector<WidgetNode*> parentStack_;
    std::unordered_map<uint32_t, WidgetNode*> idLookup_;
};

} // namespace FluentUI
