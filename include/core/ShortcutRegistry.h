#pragma once
#include "core/InputState.h"
#include "core/UIKey.h"
#include <string>
#include <functional>
#include <unordered_map>

namespace FluentUI {

// Modifier bit flags
enum ShortcutMod : uint16_t {
    MOD_NONE  = 0,
    MOD_CTRL  = 1,
    MOD_SHIFT = 2,
    MOD_ALT   = 4
};

struct KeyCombo {
    UIKey key = UIKey::Unknown;   // brief 20: platform-neutral key
    uint16_t modifiers = MOD_NONE;

    bool operator==(const KeyCombo& o) const {
        return key == o.key && modifiers == o.modifiers;
    }
};

struct ShortcutEntry {
    KeyCombo combo;
    std::function<void()> callback;
};

class ShortcutRegistry {
public:
    // Register a shortcut. Overwrites any existing entry with the same actionId.
    void Register(const std::string& actionId, KeyCombo combo, std::function<void()> callback) {
        entries_[actionId] = ShortcutEntry{combo, std::move(callback)};
    }

    void Unregister(const std::string& actionId) {
        entries_.erase(actionId);
    }

    // Call once per frame after input is updated.
    // Fires callbacks for any shortcut whose key was pressed this frame
    // while the required modifiers are held.
    void ProcessFrame(const InputState& input) {
        uint16_t currentMods = 0;
        if (input.CtrlDown())  currentMods |= MOD_CTRL;
        if (input.ShiftDown()) currentMods |= MOD_SHIFT;
        if (input.AltDown())   currentMods |= MOD_ALT;

        for (auto& [id, entry] : entries_) {
            if (entry.combo.key == UIKey::Unknown) continue;
            if (input.IsKeyPressed(entry.combo.key) && currentMods == entry.combo.modifiers) {
                if (entry.callback) entry.callback();
            }
        }
    }

    // Returns a human-readable string like "Ctrl+Shift+S"
    std::string GetShortcutText(const std::string& actionId) const {
        auto it = entries_.find(actionId);
        if (it == entries_.end()) return "";
        return ComboToString(it->second.combo);
    }

    bool HasShortcut(const std::string& actionId) const {
        return entries_.count(actionId) > 0;
    }

    static std::string ComboToString(const KeyCombo& combo) {
        std::string result;
        if (combo.modifiers & MOD_CTRL)  result += "Ctrl+";
        if (combo.modifiers & MOD_SHIFT) result += "Shift+";
        if (combo.modifiers & MOD_ALT)   result += "Alt+";

        result += UIKeyName(combo.key);
        return result;
    }

private:
    std::unordered_map<std::string, ShortcutEntry> entries_;
};

} // namespace FluentUI
