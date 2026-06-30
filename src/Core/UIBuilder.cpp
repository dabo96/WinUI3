#include "core/UIBuilder.h"
#include "UI/Widgets.h"
#include "core/Context.h"
#include "core/DockSystem.h"

namespace FluentUI {

// --- Layout containers ---

void UIBuilder::vertical(std::function<void(UIBuilder&)> content) {
    BeginVertical();
    if (content) content(*this);
    EndVertical();
}

void UIBuilder::vertical(float spacing, std::function<void(UIBuilder&)> content) {
    BeginVertical(spacing);
    if (content) content(*this);
    EndVertical();
}

void UIBuilder::horizontal(std::function<void(UIBuilder&)> content) {
    BeginHorizontal();
    if (content) content(*this);
    EndHorizontal();
}

void UIBuilder::horizontal(float spacing, std::function<void(UIBuilder&)> content) {
    BeginHorizontal(spacing);
    if (content) content(*this);
    EndHorizontal();
}

// --- Basic widgets ---

bool UIBuilder::button(const std::string& label, const Vec2& size, bool enabled) {
    return Button(label, size, std::nullopt, enabled);
}

bool UIBuilder::button(const std::string& label, uint32_t iconCodepoint,
                       const Vec2& size, bool enabled) {
    return Button(label, iconCodepoint, size, std::nullopt, enabled);
}

bool UIBuilder::iconButton(uint32_t iconCodepoint, float size, bool enabled) {
    return IconButton(iconCodepoint, size, std::nullopt, enabled);
}

void UIBuilder::label(const std::string& text, TypographyStyle variant) {
    Label(text, std::nullopt, variant);
}

void UIBuilder::label(const std::string& text, uint32_t iconCodepoint,
                      TypographyStyle variant) {
    Label(text, iconCodepoint, std::nullopt, variant);
}

void UIBuilder::iconLabel(uint32_t iconCodepoint, float size) {
    IconLabel(iconCodepoint, size);
}

void UIBuilder::separator() {
    Separator();
}

bool UIBuilder::segmentedControl(const std::string& id,
                                 const std::vector<std::string>& options,
                                 int* activeIndex) {
    return SegmentedControl(id, options, activeIndex);
}

bool UIBuilder::segmentedControl(const std::string& id,
                                 const std::vector<std::pair<std::string, uint32_t>>& options,
                                 int* activeIndex) {
    return SegmentedControl(id, options, activeIndex);
}

bool UIBuilder::checkbox(const std::string& label, bool* value) {
    return Checkbox(label, value);
}

bool UIBuilder::checkbox(const std::string& label, uint32_t iconCodepoint, bool* value) {
    return Checkbox(label, iconCodepoint, value);
}

bool UIBuilder::radioButton(const std::string& label, int* value, int option,
                            const std::string& group) {
    return RadioButton(label, value, option, group);
}

bool UIBuilder::radioButton(const std::string& label, uint32_t iconCodepoint,
                            int* value, int option, const std::string& group) {
    return RadioButton(label, iconCodepoint, value, option, group);
}

bool UIBuilder::slider(const std::string& label, float* value, float min, float max,
                       float width, const char* format) {
    return SliderFloat(label, value, min, max, width, format);
}

bool UIBuilder::slider(const std::string& label, int* value, int min, int max,
                       float width) {
    return SliderInt(label, value, min, max, width);
}

void UIBuilder::progressBar(float fraction, const Vec2& size, const std::string& overlay) {
    ProgressBar(fraction, size, overlay);
}

// --- Feedback & estado (brief 15) ---
bool UIBuilder::infoBar(const std::string& id, InfoSeverity severity,
                        const std::string& title, const std::string& message,
                        bool closable, const std::string& actionText) {
    return InfoBar(id, severity, title, message, closable, actionText);
}
void UIBuilder::progressRing(const std::string& id, float size, float progress) {
    ProgressRing(id, size, progress);
}
void UIBuilder::badge(int count, bool dot, std::optional<Vec2> anchorTopRight) {
    Badge(count, dot, anchorTopRight);
}
void UIBuilder::skeleton(const Vec2& size, float cornerRadius) {
    Skeleton(size, cornerRadius);
}
void UIBuilder::skeletonText(int lines, float lineHeight, float lastLineFraction) {
    SkeletonText(lines, lineHeight, lastLineFraction);
}

bool UIBuilder::textInput(const std::string& label, std::string* value, float width, size_t maxLength) {
    return TextInput(label, value, width, false, std::nullopt, nullptr, maxLength);
}

bool UIBuilder::comboBox(const std::string& label, int* current,
                         const std::vector<std::string>& items, float width) {
    return ComboBox(label, current, items, width);
}

bool UIBuilder::comboBox(const std::string& label, int* current,
                         const std::vector<std::pair<std::string, uint32_t>>& items, float width) {
    return ComboBox(label, current, items, width);
}

bool UIBuilder::dragFloat(const std::string& label, float* value, float speed,
                          float min, float max, const char* format) {
    return DragFloat(label, value, speed, min, max, format);
}

bool UIBuilder::dragInt(const std::string& label, int* value, float speed,
                        int min, int max) {
    return DragInt(label, value, speed, min, max);
}

bool UIBuilder::dragFloat3(const std::string& label, float values[3], float speed,
                           float min, float max, const char* format) {
    return DragFloat3(label, values, speed, min, max, format);
}

bool UIBuilder::colorPicker(const std::string& label, Color* value) {
    return ColorPicker(label, value);
}

// --- Containers ---

void UIBuilder::panel(const std::string& id, std::function<void(UIBuilder&)> content,
                      const Vec2& size) {
    if (BeginPanel(id, size)) {
        if (content) content(*this);
    }
    EndPanel();
}

void UIBuilder::panel(const std::string& id, uint32_t iconCodepoint,
                      std::function<void(UIBuilder&)> content, const Vec2& size) {
    if (BeginPanel(id, iconCodepoint, size)) {
        if (content) content(*this);
    }
    EndPanel();
}

void UIBuilder::scrollView(const std::string& id, const Vec2& size,
                           std::function<void(UIBuilder&)> content) {
    if (BeginScrollView(id, size)) {
        if (content) content(*this);
    }
    EndScrollView();
}

void UIBuilder::tabView(const std::string& id, int* activeTab,
                        const std::vector<std::string>& labels, const Vec2& size,
                        std::function<void(UIBuilder&, int activeTab)> content) {
    if (BeginTabView(id, activeTab, labels, size)) {
        int tab = activeTab ? *activeTab : 0;
        if (content) content(*this, tab);
    }
    EndTabView();
}

// --- Splitter ---

void UIBuilder::splitter(const std::string& id, bool vertical, float* ratio,
                         std::function<void(UIBuilder&)> first,
                         std::function<void(UIBuilder&)> second,
                         const Vec2& size) {
    if (BeginSplitter(id, vertical, ratio, size)) {
        if (first) first(*this);
        SplitterPanel();
        if (second) second(*this);
    }
    EndSplitter();
}

// --- Lists & Trees ---

void UIBuilder::listView(const std::string& id, const Vec2& size,
                         int* selected, const std::vector<std::string>& items) {
    if (BeginListView(id, size, selected, items)) {
        // ListView manages its own content
    }
    EndListView();
}

void UIBuilder::listView(const std::string& id, const Vec2& size,
                         std::vector<int>* selectedItems, const std::vector<std::string>& items) {
    if (BeginListView(id, size, selectedItems, items)) {
        // ListView manages its own content
    }
    EndListView();
}

void UIBuilder::treeView(const std::string& id, const Vec2& size,
                         std::function<void(UIBuilder&)> content) {
    if (BeginTreeView(id, size)) {
        if (content) content(*this);
    }
    EndTreeView();
}

bool UIBuilder::treeNode(const std::string& label, bool* isOpen,
                         std::function<void(UIBuilder&)> children) {
    bool clicked = TreeNode(label, label, isOpen);
    if (isOpen && *isOpen && children) {
        TreeNodePush();
        children(*this);
        TreeNodePop();
    }
    return clicked;
}

bool UIBuilder::treeNode(const std::string& label, uint32_t iconCodepoint,
                         bool* isOpen, std::function<void(UIBuilder&)> children) {
    bool clicked = TreeNode(label, label, iconCodepoint, isOpen);
    if (isOpen && *isOpen && children) {
        TreeNodePush();
        children(*this);
        TreeNodePop();
    }
    return clicked;
}

// --- Menu system ---

void UIBuilder::menuBar(std::function<void(UIBuilder&)> content) {
    if (BeginMenuBar()) {
        if (content) content(*this);
        EndMenuBar();
    }
}

void UIBuilder::menu(const std::string& label, std::function<void(UIBuilder&)> content) {
    if (BeginMenu(label)) {
        if (content) content(*this);
        EndMenu();
    }
}

void UIBuilder::menu(const std::string& label, uint32_t iconCodepoint,
                     std::function<void(UIBuilder&)> content) {
    if (BeginMenu(label, iconCodepoint)) {
        if (content) content(*this);
        EndMenu();
    }
}

bool UIBuilder::menuItem(const std::string& label, bool enabled) {
    return MenuItem(label, enabled);
}

bool UIBuilder::menuItem(const std::string& label, uint32_t iconCodepoint, bool enabled) {
    return MenuItem(label, iconCodepoint, enabled);
}

void UIBuilder::menuSeparator() {
    MenuSeparator();
}

// --- Toolbar & StatusBar ---

void UIBuilder::toolbar(std::function<void(UIBuilder&)> content) {
    BeginToolbar();
    if (content) content(*this);
    EndToolbar();
}

void UIBuilder::statusBar(const std::string& text, std::function<void(UIBuilder&)> content) {
    BeginStatusBar(text);
    if (content) content(*this);
    EndStatusBar();
}

// --- Overlays ---

void UIBuilder::tooltip(const std::string& text, float delay) {
    Tooltip(text, delay);
}

void UIBuilder::contextMenu(const std::string& id, std::function<void(UIBuilder&)> content) {
    if (BeginContextMenu(id)) {
        if (content) content(*this);
    }
    EndContextMenu();
}

void UIBuilder::modal(const std::string& id, const std::string& title, bool* open,
                      const Vec2& size, std::function<void(UIBuilder&)> content) {
    if (BeginModal(id, title, open, size)) {
        if (content) content(*this);
    }
    EndModal();
}

void UIBuilder::flyout(const std::string& id, const Rect& anchorRect,
                       std::function<void(UIBuilder&)> content) {
    // EndFlyout only when open: BeginFlyout returning false leaves no flyout scope
    // (and could otherwise act on an unrelated active flyout).
    if (BeginFlyout(id, anchorRect)) {
        if (content) content(*this);
        EndFlyout();
    }
}

void UIBuilder::menuFlyout(const std::string& id, const Rect& anchorRect,
                           const std::vector<MenuEntry>& entries) {
    MenuFlyout(id, anchorRect, entries);
}

// --- BRIEF 14: Signature controls (sugar) ---

bool UIBuilder::toggleSwitch(const std::string& label, bool* value,
                             const std::string& onText, const std::string& offText) {
    return ToggleSwitch(label, value, onText, offText);
}

void UIBuilder::expander(const std::string& id, const std::string& header,
                         std::function<void(UIBuilder&)> content,
                         uint32_t icon, bool* expanded) {
    if (BeginExpander(id, header, icon, expanded)) {
        if (content) content(*this);
        EndExpander();
    }
}

int UIBuilder::splitButton(const std::string& label, uint32_t icon,
                           std::function<void()> onPrimary,
                           const std::vector<CommandItem>& menu) {
    return SplitButton(label, icon, onPrimary, menu);
}

void UIBuilder::dropDownButton(const std::string& label, uint32_t icon,
                               const std::vector<CommandItem>& menu) {
    DropDownButton(label, icon, menu);
}

bool UIBuilder::numberBox(const std::string& label, double* value,
                          double min, double max, double step, const char* format) {
    return NumberBox(label, value, min, max, step, format);
}

bool UIBuilder::teachingTip(const std::string& id, const Rect& targetRect,
                            const std::string& title, const std::string& body,
                            const std::string& actionText) {
    return TeachingTip(id, targetRect, title, body, actionText);
}

DialogResult UIBuilder::contentDialog(const std::string& id, bool* open,
                                      const std::string& title,
                                      std::function<void(UIBuilder&)> body,
                                      const std::string& primaryText,
                                      const std::string& secondaryText,
                                      const std::string& closeText) {
    return ContentDialog(id, open, title,
                         [&]() { if (body) body(*this); },
                         primaryText, secondaryText, closeText);
}

bool UIBuilder::rating(const std::string& id, int* value, int maxStars,
                       bool allowHalf) {
    return RatingControl(id, value, maxStars, allowHalf);
}

// --- Grid layout ---

void UIBuilder::grid(const std::string& id, int columns, int itemCount,
                     std::function<void(UIBuilder&, int index)> cellContent,
                     float rowHeight) {
    BeginGrid(id, columns, rowHeight);
    for (int i = 0; i < itemCount; ++i) {
        if (i > 0) GridNextCell();
        if (cellContent) cellContent(*this, i);
    }
    EndGrid();
}

// --- Layout primitives (brief 19) ---

void UIBuilder::wrapPanel(const std::string& id,
                          std::function<void(UIBuilder&)> content,
                          float hGap, float vGap) {
    BeginWrapPanel(id, hGap, vGap);
    if (content) content(*this);
    EndWrapPanel();
}

void UIBuilder::uniformGrid(const std::string& id, int columns, int itemCount,
                            std::function<void(UIBuilder&, int index)> cellContent,
                            float gap) {
    BeginUniformGrid(id, columns, gap);
    for (int i = 0; i < itemCount; ++i) {
        if (i > 0) UniformGridNextCell();
        if (cellContent) cellContent(*this, i);
    }
    EndUniformGrid();
}

void UIBuilder::canvas(const std::string& id, const Vec2& size,
                       std::function<void(UIBuilder&)> content) {
    BeginCanvas(id, size);
    if (content) content(*this);
    EndCanvas();
}

void UIBuilder::adaptiveLayout(std::function<void(UIBuilder&, Breakpoint)> build) {
    if (!build) return;
    AdaptiveLayout([this, &build](Breakpoint bp) { build(*this, bp); });
}

// --- Table/DataGrid ---

void UIBuilder::table(const std::string& id, std::vector<TableColumn>& columns,
                      int rowCount, const Vec2& size,
                      std::function<void(UIBuilder&, int row, int col)> cellContent,
                      TableState* state) {
    if (BeginTable(id, columns, rowCount, size, state)) {
        // Get the table frame context to know visible row range
        UIContext* c = context();
        if (c && !c->tableStack.empty()) {
            const auto& frame = c->tableStack.back();
            int startRow = frame.startVisibleRow;
            int endRow = frame.endVisibleRow;
            int numCols = static_cast<int>(columns.size());

            // Only iterate visible rows (virtual scrolling)
            for (int row = startRow; row < endRow; ++row) {
                TableNextRow();
                for (int col = 0; col < numCols; ++col) {
                    TableSetCell(col);
                    if (cellContent) cellContent(*this, row, col);
                }
            }
        }
    }
    EndTable();
}

// --- Image ---

void UIBuilder::image(const std::string& id, void* textureHandle, const Vec2& size,
                      const Vec2& uv0, const Vec2& uv1) {
    Image(id, textureHandle, size, uv0, uv1);
}

// --- Layout helpers ---

void UIBuilder::spacing(float px) {
    Spacing(px);
}

void UIBuilder::sameLine(float offset) {
    SameLine(offset);
}

void UIBuilder::setNextSize(float w, float h) {
    SetNextConstraints(FixedSize(w, h));
}

void UIBuilder::setNextConstraints(const LayoutConstraints& c) {
    SetNextConstraints(c);
}

// --- Style overrides (Phase 6) ---

void UIBuilder::pushStyle(const Style& override) {
    if (ctx) ctx->styleStack.push_back(override);
}

void UIBuilder::popStyle() {
    if (ctx && !ctx->styleStack.empty()) ctx->styleStack.pop_back();
}

void UIBuilder::pushButtonStyle(const ButtonStyle& s) {
    if (ctx) ctx->buttonStyleStack.push_back(s);
}

void UIBuilder::popButtonStyle() {
    if (ctx && !ctx->buttonStyleStack.empty()) ctx->buttonStyleStack.pop_back();
}

void UIBuilder::pushPanelStyle(const PanelStyle& s) {
    if (ctx) ctx->panelStyleStack.push_back(s);
}

void UIBuilder::popPanelStyle() {
    if (ctx && !ctx->panelStyleStack.empty()) ctx->panelStyleStack.pop_back();
}

void UIBuilder::pushTextColor(const Color& color) {
    if (ctx) ctx->textColorStack.push_back(color);
}

void UIBuilder::popTextColor() {
    if (ctx && !ctx->textColorStack.empty()) ctx->textColorStack.pop_back();
}

// --- Debug Overlay (Phase C) ---

void UIBuilder::debugOverlay() {
    if (!ctx || !ctx->showDebugOverlay) return;

    auto& p = ctx->perfCounters;
    Vec2 viewport = ctx->renderer.GetViewportSize();

    // Draw semi-transparent background in top-right corner
    float overlayW = 260.0f;
    float overlayH = 200.0f;
    Vec2 overlayPos = {viewport.x - overlayW - 10.0f, 10.0f};

    ctx->renderer.SetLayer(RenderLayer::Tooltip); // Draw on top of everything
    ctx->renderer.DrawRectFilled(overlayPos, {overlayW, overlayH},
                                  Color(0.0f, 0.0f, 0.0f, 0.85f), 6.0f);
    ctx->renderer.DrawRect(overlayPos, {overlayW, overlayH},
                           Color(0.3f, 0.6f, 1.0f, 0.6f), 6.0f);

    float x = overlayPos.x + 8.0f;
    float y = overlayPos.y + 6.0f;
    float lineH = 14.0f;
    float fontSize = 11.0f;
    Color white(1.0f, 1.0f, 1.0f, 0.95f);
    Color accent(0.4f, 0.7f, 1.0f, 1.0f);
    Color dim(0.7f, 0.7f, 0.7f, 0.8f);

    auto drawLine = [&](const std::string& label, const std::string& value, Color valColor = {1,1,1,0.95f}) {
        ctx->renderer.DrawText({x, y}, label, dim, fontSize);
        ctx->renderer.DrawText({x + 140.0f, y}, value, valColor, fontSize);
        y += lineH;
    };

    ctx->renderer.DrawText({x, y}, "FluentUI Performance", accent, 12.0f);
    y += lineH + 2.0f;

    drawLine("Batches:", std::to_string(p.batchCount));
    drawLine("Draw calls:", std::to_string(p.drawCalls));
    drawLine("Vertices:", std::to_string(p.vertexCount));
    drawLine("Indices:", std::to_string(p.indexCount));
    drawLine("Flushes:", std::to_string(p.flushCount));
    drawLine("Batch merges:", std::to_string(p.batchMerges));
    drawLine("State changes:", std::to_string(p.stateChanges));
    drawLine("Clip pushes:", std::to_string(p.clipPushes));

    uint32_t totalTextLookups = p.textCacheHits + p.textCacheMisses;
    float hitRate = totalTextLookups > 0 ? (100.0f * p.textCacheHits / totalTextLookups) : 0.0f;
    char hitRateStr[32];
    snprintf(hitRateStr, sizeof(hitRateStr), "%u/%u (%.0f%%)", p.textCacheHits, totalTextLookups, hitRate);
    drawLine("Text cache:", hitRateStr, hitRate > 90.0f ? Color(0.3f, 1.0f, 0.3f, 1.0f) : Color(1.0f, 0.5f, 0.3f, 1.0f));

    drawLine("Color anims:", std::to_string(p.activeColorAnims));
    drawLine("Widget nodes:", std::to_string(p.widgetNodeCount));

    char fpsStr[16];
    snprintf(fpsStr, sizeof(fpsStr), "%.1f", ctx->deltaTime > 0 ? 1.0f / ctx->deltaTime : 0.0f);
    drawLine("FPS:", fpsStr, accent);

    ctx->renderer.SetLayer(RenderLayer::Default); // Restore
}

// --- Dock Space (Phase 4) ---

void UIBuilder::dockSpace(std::function<void(UIBuilder&)> content) {
    if (!ctx) return;

    // Compute available area for the dock space
    Vec2 viewport = ctx->renderer.GetViewportSize();
    Vec2 pos = ctx->cursorPos;
    Vec2 size = {viewport.x - pos.x, viewport.y - pos.y};

    // Set dock space bounds and compute layout
    Rect dockArea{pos, size};
    ctx->dockSpace.ComputeLayout(dockArea);

    // Call the content lambda — user will call dockPanel() inside
    if (content) content(*this);

    // Draw split dividers ONCE after all panels have been rendered
    auto& dock = ctx->dockSpace;
    auto* root = dock.Root();
    if (root) {
        std::function<void(DockNode*)> drawDividers = [&](DockNode* node) {
            if (!node || node->type != DockNode::Type::Split) return;
            if (!node->first || !node->second) return;

            float divider = DockSpace::DIVIDER_THICKNESS;
            Vec2 divPos, divSize;

            if (node->splitVertical) {
                float firstW = (node->bounds.size.x - divider) * node->splitRatio;
                divPos = {node->bounds.pos.x + firstW, node->bounds.pos.y};
                divSize = {divider, node->bounds.size.y};
            } else {
                float firstH = (node->bounds.size.y - divider) * node->splitRatio;
                divPos = {node->bounds.pos.x, node->bounds.pos.y + firstH};
                divSize = {node->bounds.size.x, divider};
            }

            // Draw divider background
            Color divColor = node->resizing
                ? Color(0.4f, 0.6f, 1.0f, 0.8f)
                : Color(0.25f, 0.25f, 0.25f, 1.0f);
            ctx->renderer.DrawRectFilled(divPos, divSize, divColor);

            // Draw grip dots
            Vec2 center = {divPos.x + divSize.x * 0.5f, divPos.y + divSize.y * 0.5f};
            float dotSize = 2.0f;
            Color dotColor(0.5f, 0.5f, 0.5f, 0.8f);
            for (int i = -1; i <= 1; ++i) {
                Vec2 dotPos;
                if (node->splitVertical) {
                    dotPos = {center.x, center.y + i * 6.0f};
                } else {
                    dotPos = {center.x + i * 6.0f, center.y};
                }
                ctx->renderer.DrawCircle(dotPos, dotSize, dotColor, true);
            }

            // Set cursor for resize
            if (Rect{divPos, divSize}.Contains({ctx->input.MouseX(), ctx->input.MouseY()})) {
                ctx->desiredCursor = node->splitVertical
                    ? UIContext::CursorType::ResizeH
                    : UIContext::CursorType::ResizeV;
            }

            drawDividers(node->first.get());
            drawDividers(node->second.get());
        };
        drawDividers(root);
    }

    // --- Dock Drag Visual Feedback ---
    {
        auto& drag = ctx->dockDrag;
        float mx = ctx->input.MouseX(), my = ctx->input.MouseY();
        bool mouseDown = ctx->input.IsMouseDown(0);

        if (drag.isDragging) {
            if (!mouseDown) {
                // Phase E3: detect drop outside the window viewport — fire drag-out callback.
                bool outsideViewport = (mx < 0.0f || my < 0.0f ||
                                        mx > viewport.x || my > viewport.y);
                if (outsideViewport && drag.onPanelDragOut) {
                    float gx = 0.0f, gy = 0.0f;
                    SDL_GetGlobalMouseState(&gx, &gy);
                    std::string panelId = drag.panelId;
                    auto cb = drag.onPanelDragOut;
                    drag.Reset();
                    cb(panelId, static_cast<int>(gx), static_cast<int>(gy));
                } else if (drag.hoverZone != DockPosition::Float && !drag.hoverTargetId.empty()) {
                    // Drop: dock at hovered zone
                    dock.UndockPanel(drag.panelId);
                    dock.DockPanel(drag.panelId, drag.hoverZone, drag.hoverTargetId);
                    dock.ComputeLayout(dockArea);
                    drag.Reset();
                } else {
                    drag.Reset();
                }
            } else {
                // Update ghost position
                drag.ghostPos = {mx - drag.dragOffset.x, my - drag.dragOffset.y};

                // Hit test dock zones against all panels
                drag.hoverZone = DockPosition::Float;
                drag.hoverTargetId.clear();
                drag.showZones = false;

                auto panels = dock.GetDockedPanels();
                for (auto& pid : panels) {
                    if (pid == drag.panelId) continue;
                    Rect panelBounds = dock.GetPanelBounds(pid);
                    DockPosition zone = HitTestDockZones(panelBounds, mx, my);
                    if (zone != DockPosition::Float) {
                        drag.hoverZone = zone;
                        drag.hoverTargetId = pid;
                        drag.showZones = true;
                        break;
                    }
                }

                // Draw ghost panel (translucent)
                Color ghostColor = ctx->style.button.background.normal;
                ghostColor.a = 0.3f;
                ctx->renderer.DrawRectFilled(drag.ghostPos, drag.ghostSize, ghostColor, 6.0f);
                ctx->renderer.DrawRect(drag.ghostPos, drag.ghostSize,
                                       Color(0.4f, 0.6f, 1.0f, 0.6f), 6.0f);

                // Draw dock zone preview (blue highlight)
                if (drag.showZones && !drag.hoverTargetId.empty()) {
                    Rect targetBounds = dock.GetPanelBounds(drag.hoverTargetId);
                    Rect preview = GetDockZonePreviewRect(targetBounds, drag.hoverZone);
                    Color zoneColor(0.2f, 0.4f, 0.9f, 0.25f);
                    ctx->renderer.DrawRectFilled(preview.pos, preview.size, zoneColor, 4.0f);
                    Color zoneBorder(0.3f, 0.5f, 1.0f, 0.6f);
                    ctx->renderer.DrawRect(preview.pos, preview.size, zoneBorder, 4.0f);
                }

                // Set hand cursor while dragging
                ctx->desiredCursor = UIContext::CursorType::Hand;
            }
        }
    }

    // Advance cursor past the dock space so subsequent widgets don't overlap
    ctx->cursorPos.y = pos.y + size.y;
}

void UIBuilder::dockPanel(const std::string& panelId, std::function<void(UIBuilder&)> content) {
    if (!ctx) return;

    auto& dock = ctx->dockSpace;
    if (dock.IsEmpty()) return;

    // Find this panel's bounds in the dock tree
    Rect bounds = dock.GetPanelBounds(panelId);
    if (bounds.size.x <= 0 || bounds.size.y <= 0) return;

    // Render a panel at the computed dock bounds
    // Draw panel background
    const auto& style = ctx->style;
    ctx->renderer.DrawRectFilled(bounds.pos, bounds.size,
                                  style.panel.background, style.panel.cornerRadius);
    ctx->renderer.DrawRect(bounds.pos, bounds.size,
                           style.panel.borderColor, style.panel.cornerRadius);

    // Draw title bar with drag handle
    float titleHeight = 28.0f;
    Rect titleRect{bounds.pos, {bounds.size.x, titleHeight}};
    ctx->renderer.DrawRectFilled(titleRect.pos, titleRect.size,
                                  style.panel.headerBackground, style.panel.cornerRadius);
    ctx->renderer.DrawText(
        {bounds.pos.x + 8.0f, bounds.pos.y + 6.0f},
        panelId, style.panel.headerText.color, style.panel.headerText.fontSize);

    // Drag handle: initiate dock drag from title bar
    {
        float mx = ctx->input.MouseX(), my = ctx->input.MouseY();
        bool hoverTitle = titleRect.Contains({mx, my});
        bool mousePressed = ctx->input.IsMousePressed(0);
        bool mouseDown = ctx->input.IsMouseDown(0);

        if (!ctx->dockDrag.isDragging && hoverTitle && mousePressed) {
            ctx->dockDrag.isDragging = true;
            ctx->dockDrag.panelId = panelId;
            ctx->dockDrag.dragOffset = {mx - bounds.pos.x, my - bounds.pos.y};
            ctx->dockDrag.ghostSize = bounds.size;
        }
    }

    // Set up clipping and layout for content
    Vec2 contentPos = {bounds.pos.x, bounds.pos.y + titleHeight};
    Vec2 contentSize = {bounds.size.x, bounds.size.y - titleHeight};

    ctx->renderer.PushClipRect(contentPos, contentSize);

    // Push a layout stack for the content
    Vec2 savedCursor = ctx->cursorPos;
    ctx->cursorPos = {contentPos.x + style.panel.padding.x, contentPos.y + style.panel.padding.y};

    Vec2 innerSize = {contentSize.x - style.panel.padding.x * 2.0f,
                      contentSize.y - style.panel.padding.y * 2.0f};
    BeginVertical(-1.0f, innerSize);

    if (content) content(*this);

    EndVertical();

    ctx->cursorPos = savedCursor;
    ctx->renderer.PopClipRect();
}

// --- DPI helpers (Phase 4) ---

float UIBuilder::dpiScale() const {
    return ctx ? ctx->dpiScale : 1.0f;
}

float UIBuilder::scaled(float value) const {
    return ctx ? value * ctx->dpiScale : value;
}

// ─── BRIEF 16: Collections (sugar) ──────────────────────────────────────────

void UIBuilder::gridView(const std::string& id, int itemCount, const Vec2& itemSize,
                         std::function<void(UIBuilder&, int)> itemBuilder,
                         float gap, float minItemWidth) {
    GridView(id, itemCount, itemSize,
             [this, &itemBuilder](int index) { if (itemBuilder) itemBuilder(*this, index); },
             gap, minItemWidth);
}

DataGridResult UIBuilder::dataGrid(const std::string& id,
                                   const std::vector<DataColumn>& cols, int rowCount,
                                   std::function<std::string(int, int)> getCell,
                                   std::function<void(int, int, const std::string&)> setCell) {
    return DataGrid(id, cols, rowCount, getCell, setCell);
}

int UIBuilder::pagination(const std::string& id, int pageCount, int* currentPage) {
    return Pagination(id, pageCount, currentPage);
}

void UIBuilder::expanderList(const std::string& id, int itemCount,
                             std::function<std::string(int)> headerFn,
                             std::function<void(UIBuilder&, int)> bodyFn,
                             bool accordion) {
    ExpanderList(id, itemCount, headerFn,
                 [this, &bodyFn](int i) { if (bodyFn) bodyFn(*this, i); }, accordion);
}

int UIBuilder::flipView(const std::string& id, int itemCount,
                        std::function<void(UIBuilder&, int)> itemBuilder,
                        int* currentIndex) {
    return FlipView(id, itemCount,
                    [this, &itemBuilder](int index) { if (itemBuilder) itemBuilder(*this, index); },
                    currentIndex);
}

// ─── BRIEF 13: App shell & navegación (sugar) ───────────────────────────────

std::string UIBuilder::navigationView(const std::string& id,
                                      const std::vector<NavItem>& items,
                                      std::string* selectedKey, NavDisplayMode mode,
                                      const std::vector<NavItem>& footerItems) {
    return NavigationView(id, items, selectedKey, mode, footerItems);
}

std::string UIBuilder::navigationView(const std::string& id,
                                      const std::vector<NavItem>& items,
                                      std::string* selectedKey) {
    return NavigationView(id, items, selectedKey);
}

void UIBuilder::commandBar(const std::string& id,
                           const std::vector<CommandItem>& primary,
                           const std::vector<CommandItem>& secondary) {
    CommandBar(id, primary, secondary);
}

int UIBuilder::breadcrumbBar(const std::string& id,
                             const std::vector<std::string>& crumbs) {
    return BreadcrumbBar(id, crumbs);
}

TitleBarResult UIBuilder::titleBar(const std::string& id, const std::string& title,
                                   uint32_t icon, std::function<void()> centerContent) {
    return TitleBar(id, title, icon, std::move(centerContent));
}

} // namespace FluentUI
