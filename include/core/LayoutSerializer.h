#pragma once
#include "core/DockSystem.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

namespace FluentUI {

// Forward declarations
struct UIContext;

// Simple key-value layout serialization format
// Format is a custom text-based format (no JSON library dependency)
//
// Example output:
// [DockLayout]
// type=split
// vertical=1
// ratio=0.25
// [DockLayout.first]
// type=leaf
// panel=Hierarchy
// [DockLayout.second]
// type=leaf
// panel=Viewport
// [Panels]
// panel.MyPanel.x=100
// panel.MyPanel.y=200
// ...

class LayoutSerializer {
public:
    // Save the current dock layout and panel states to a file
    static bool SaveLayout(const std::string& filepath, const DockSpace& dockSpace,
                           UIContext* ctx = nullptr);

    // Load dock layout and panel states from a file
    static bool LoadLayout(const std::string& filepath, DockSpace& dockSpace,
                           UIContext* ctx = nullptr);

    // Serialize dock tree to string
    static std::string SerializeDockTree(const DockNode* node, const std::string& prefix = "dock");

    // Deserialize dock tree from key-value map
    static std::unique_ptr<DockNode> DeserializeDockTree(
        const std::unordered_map<std::string, std::string>& data,
        const std::string& prefix = "dock");

private:
    static void WriteDockNode(std::ostream& out, const DockNode* node, const std::string& prefix);
    static std::unique_ptr<DockNode> ReadDockNode(
        const std::unordered_map<std::string, std::string>& data,
        const std::string& prefix);
};

} // namespace FluentUI
