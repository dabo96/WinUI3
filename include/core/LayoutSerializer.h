#pragma once
#include "core/DockSystem.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace FluentUI {

// Forward declarations
struct UIContext;

// Phase E5: information about a detached dock panel hosted in its own OS window.
// Captured during SaveLayout for later restoration.
struct ViewportInfo {
    std::string panelId;
    int x = 0;
    int y = 0;
    int width = 480;
    int height = 360;
};

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

    // Phase E5: save layout including detached viewports (multi-viewport).
    static bool SaveLayout(const std::string& filepath, const DockSpace& dockSpace,
                           UIContext* ctx, const std::vector<ViewportInfo>& viewports);

    // Phase E5: load layout and populate viewports list (caller restores them
    // via FluentApp::restoreViewports with a builder factory).
    static bool LoadLayout(const std::string& filepath, DockSpace& dockSpace,
                           UIContext* ctx, std::vector<ViewportInfo>* viewports);

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
