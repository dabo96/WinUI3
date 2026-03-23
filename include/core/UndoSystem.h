#pragma once
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace FluentUI {

// Command pattern for undo/redo
struct Command {
    std::string description;
    std::function<void()> execute;
    std::function<void()> undo;
};

class UndoStack {
public:
    // Execute a command and push it onto the stack
    void Execute(Command cmd) {
        if (cmd.execute) cmd.execute();
        // Erase any redo history after current position
        if (currentIndex_ + 1 < static_cast<int>(history_.size())) {
            history_.erase(history_.begin() + currentIndex_ + 1, history_.end());
        }
        history_.push_back(std::move(cmd));
        currentIndex_ = static_cast<int>(history_.size()) - 1;
        // Enforce max history size
        if (history_.size() > maxSize_) {
            history_.erase(history_.begin());
            currentIndex_--;
        }
    }

    // Undo the last command
    bool Undo() {
        if (!CanUndo()) return false;
        auto& cmd = history_[currentIndex_];
        if (cmd.undo) cmd.undo();
        currentIndex_--;
        return true;
    }

    // Redo the last undone command
    bool Redo() {
        if (!CanRedo()) return false;
        currentIndex_++;
        auto& cmd = history_[currentIndex_];
        if (cmd.execute) cmd.execute();
        return true;
    }

    bool CanUndo() const { return currentIndex_ >= 0; }
    bool CanRedo() const { return currentIndex_ + 1 < static_cast<int>(history_.size()); }

    // Get description of current/next undo/redo
    std::string UndoDescription() const {
        if (!CanUndo()) return "";
        return history_[currentIndex_].description;
    }
    std::string RedoDescription() const {
        if (!CanRedo()) return "";
        return history_[currentIndex_ + 1].description;
    }

    // Clear all history
    void Clear() {
        history_.clear();
        currentIndex_ = -1;
    }

    // Get history size
    size_t Size() const { return history_.size(); }
    int CurrentIndex() const { return currentIndex_; }

    // Set max history size (default 100)
    void SetMaxSize(size_t maxSize) { maxSize_ = maxSize; }

    // Begin/End group — multiple commands collapsed into one undo step
    void BeginGroup(const std::string& description) {
        groupDescription_ = description;
        groupCommands_.clear();
        inGroup_ = true;
    }

    void EndGroup() {
        if (!inGroup_ || groupCommands_.empty()) {
            inGroup_ = false;
            return;
        }
        inGroup_ = false;
        auto cmds = std::move(groupCommands_);
        std::string desc = groupDescription_;
        Execute({
            desc,
            [cmds]() { for (auto& c : cmds) if (c.execute) c.execute(); },
            [cmds]() { for (auto it = cmds.rbegin(); it != cmds.rend(); ++it) if (it->undo) it->undo(); }
        });
    }

    // Add command within a group (does not execute immediately, just records)
    void AddToGroup(Command cmd) {
        if (inGroup_) {
            if (cmd.execute) cmd.execute();
            groupCommands_.push_back(std::move(cmd));
        } else {
            Execute(std::move(cmd));
        }
    }

private:
    std::vector<Command> history_;
    int currentIndex_ = -1;
    size_t maxSize_ = 100;

    // Group support
    bool inGroup_ = false;
    std::string groupDescription_;
    std::vector<Command> groupCommands_;
};

// Helper: create a value-change command for simple types
template<typename T>
Command MakeValueCommand(const std::string& description, T* target, T newValue) {
    T oldValue = *target;
    return {
        description,
        [target, newValue]() { *target = newValue; },
        [target, oldValue]() { *target = oldValue; }
    };
}

} // namespace FluentUI
