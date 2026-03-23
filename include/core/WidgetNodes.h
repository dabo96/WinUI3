#pragma once
#include "core/WidgetNode.h"
#include <map>

namespace FluentUI {

// --- PanelNode ---
class PanelNode : public WidgetNode {
public:
    Vec2 position{0.0f, 0.0f};
    Vec2 size{300.0f, 200.0f};
    Vec2 reservedLayoutSize{0.0f, 0.0f};
    Vec2 expandedLayoutSize{300.0f, 200.0f};
    bool initialized = false;
    bool minimized = false;
    bool dragging = false;
    Vec2 dragOffset{0.0f, 0.0f};
    bool resizing = false;
    Vec2 resizeStartMouse{0.0f, 0.0f};
    Vec2 resizeStartSize{0.0f, 0.0f};
    bool useAcrylic = false;
    float acrylicOpacity = 0.85f;
    bool useAbsolutePos = false;
    Vec2 absolutePos{0.0f, 0.0f};
    Vec2 dragPositionOffset{0.0f, 0.0f};
    bool hasBeenDragged = false;
    Vec2 contentSize{0.0f, 0.0f};

    // Scroll support
    Vec2 scrollOffset{0.0f, 0.0f};
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;

    PanelNode() { nodeType = WidgetNodeType::Panel; }
    explicit PanelNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::Panel) {}
};

// --- ScrollViewNode ---
class ScrollViewNode : public WidgetNode {
public:
    Vec2 scrollOffset{0.0f, 0.0f};
    Vec2 contentSize{0.0f, 0.0f};
    Vec2 viewSize{0.0f, 0.0f};
    bool initialized = false;
    bool draggingScrollbar = false;
    int draggingScrollbarType = 0; // 0=none, 1=vertical, 2=horizontal
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll{0.0f};
    bool useAbsolutePos = false;
    Vec2 absolutePos{0.0f, 0.0f};
    Vec2 position{0.0f, 0.0f};

    ScrollViewNode() { nodeType = WidgetNodeType::ScrollView; }
    explicit ScrollViewNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::ScrollView) {}
};

// --- TabViewNode ---
class TabViewNode : public WidgetNode {
public:
    int activeTab = 0;
    Vec2 tabBarSize{0.0f, 0.0f};
    bool initialized = false;
    std::map<int, Vec2> tabScrollOffsets;
    Vec2 contentSize{0.0f, 0.0f};
    Vec2 viewSize{0.0f, 0.0f};
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;
    bool useAbsolutePos = false;
    Vec2 absolutePos{0.0f, 0.0f};

    TabViewNode() { nodeType = WidgetNodeType::TabView; }
    explicit TabViewNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::TabView) {}
};

// --- ListViewNode ---
class ListViewNode : public WidgetNode {
public:
    int selectedItem = -1;
    std::vector<int> selectedItems; // Multi-select support (Phase 3)
    Vec2 itemSize{0.0f, 32.0f};
    bool initialized = false;
    bool useAbsolutePos = false;
    Vec2 absolutePos{0.0f, 0.0f};
    float scrollOffset = 0.0f;
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;

    ListViewNode() { nodeType = WidgetNodeType::ListView; }
    explicit ListViewNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::ListView) {}
};

// --- TreeViewNode ---
class TreeViewNode : public WidgetNode {
public:
    Vec2 itemSize{0.0f, 24.0f};
    float indentSize = 20.0f;
    float expandButtonSize = 14.0f;
    bool initialized = false;
    bool useAbsolutePos = false;
    Vec2 absolutePos{0.0f, 0.0f};

    TreeViewNode() { nodeType = WidgetNodeType::TreeView; }
    explicit TreeViewNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::TreeView) {}
};

// --- ModalNode ---
class ModalNode : public WidgetNode {
public:
    bool open = false;
    Vec2 position{0.0f, 0.0f};
    Vec2 size{400.0f, 300.0f};
    Vec2 minSize{400.0f, 300.0f};
    Vec2 contentSize{0.0f, 0.0f};
    bool dragging = false;
    Vec2 dragOffset{0.0f, 0.0f};
    bool initialized = false;

    ModalNode() { nodeType = WidgetNodeType::Modal; }
    explicit ModalNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::Modal) {}
};

// --- MenuBarNode ---
class MenuBarNode : public WidgetNode {
public:
    Vec2 position{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};
    bool initialized = false;

    MenuBarNode() { nodeType = WidgetNodeType::MenuBar; }
    explicit MenuBarNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::MenuBar) {}
};

// --- SplitterNode (Phase 3 placeholder) ---
class SplitterNode : public WidgetNode {
public:
    float ratio = 0.5f;
    bool isVertical = true;

    SplitterNode() { nodeType = WidgetNodeType::Splitter; }
    explicit SplitterNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::Splitter) {}
};

// --- TableNode ---
class TableNode : public WidgetNode {
public:
    float scrollOffset = 0.0f;
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;
    int sortColumn = -1;
    bool sortAscending = true;
    // Column widths are persisted via the user's TableColumn vector

    TableNode() { nodeType = WidgetNodeType::Table; }
    explicit TableNode(uint32_t nodeId)
        : WidgetNode(nodeId, WidgetNodeType::Table) {}
};

} // namespace FluentUI
