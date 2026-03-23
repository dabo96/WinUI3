#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include "UI/Layout.h"
#include "Theme/Style.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace FluentUI {

// Forward declarations
struct UIContext;

class UIBuilder {
public:
    explicit UIBuilder(UIContext* context) : ctx(context) {}

    // --- Layout containers (lambda-based) ---
    void vertical(std::function<void(UIBuilder&)> content);
    void vertical(float spacing, std::function<void(UIBuilder&)> content);
    void horizontal(std::function<void(UIBuilder&)> content);
    void horizontal(float spacing, std::function<void(UIBuilder&)> content);

    // --- Basic widgets ---
    bool button(const std::string& label, const Vec2& size = {0, 0}, bool enabled = true);
    void label(const std::string& text, TypographyStyle variant = TypographyStyle::Body);
    void separator();
    bool checkbox(const std::string& label, bool* value);
    bool radioButton(const std::string& label, int* value, int option,
                     const std::string& group = "");
    bool slider(const std::string& label, float* value, float min, float max,
                float width = 200.0f, const char* format = "%.2f");
    bool slider(const std::string& label, int* value, int min, int max,
                float width = 200.0f);
    void progressBar(float fraction, const Vec2& size = {0, 0},
                     const std::string& overlay = "");
    bool textInput(const std::string& label, std::string* value, float width = 200.0f, size_t maxLength = 0);
    bool comboBox(const std::string& label, int* current,
                  const std::vector<std::string>& items, float width = 200.0f);
    bool dragFloat(const std::string& label, float* value, float speed = 1.0f,
                   float min = 0.0f, float max = 0.0f, const char* format = "%.2f");
    bool dragInt(const std::string& label, int* value, float speed = 1.0f,
                 int min = 0, int max = 0);
    bool dragFloat3(const std::string& label, float values[3], float speed = 1.0f,
                    float min = 0.0f, float max = 0.0f, const char* format = "%.2f");
    bool colorPicker(const std::string& label, Color* value);

    // --- Containers (lambda-based) ---
    void panel(const std::string& id, std::function<void(UIBuilder&)> content,
               const Vec2& size = {0, 0});
    void scrollView(const std::string& id, const Vec2& size,
                    std::function<void(UIBuilder&)> content);
    void tabView(const std::string& id, int* activeTab,
                 const std::vector<std::string>& labels, const Vec2& size,
                 std::function<void(UIBuilder&, int activeTab)> content);

    // --- Splitter ---
    void splitter(const std::string& id, bool vertical, float* ratio,
                  std::function<void(UIBuilder&)> first,
                  std::function<void(UIBuilder&)> second,
                  const Vec2& size = {0, 0});

    // --- Lists & Trees ---
    void listView(const std::string& id, const Vec2& size,
                  int* selected, const std::vector<std::string>& items);
    void listView(const std::string& id, const Vec2& size,
                  std::vector<int>* selectedItems, const std::vector<std::string>& items);
    void treeView(const std::string& id, const Vec2& size,
                  std::function<void(UIBuilder&)> content);
    bool treeNode(const std::string& label, bool* isOpen = nullptr,
                  std::function<void(UIBuilder&)> children = nullptr);

    // --- Menu system ---
    void menuBar(std::function<void(UIBuilder&)> content);
    void menu(const std::string& label, std::function<void(UIBuilder&)> content);
    bool menuItem(const std::string& label, bool enabled = true);
    void menuSeparator();

    // --- Toolbar & StatusBar ---
    void toolbar(std::function<void(UIBuilder&)> content);
    void statusBar(const std::string& text = "", std::function<void(UIBuilder&)> content = nullptr);

    // --- Overlays ---
    void tooltip(const std::string& text, float delay = 0.5f);
    void contextMenu(const std::string& id, std::function<void(UIBuilder&)> content);
    void modal(const std::string& id, const std::string& title, bool* open,
               const Vec2& size, std::function<void(UIBuilder&)> content);

    // --- Grid layout ---
    void grid(const std::string& id, int columns, int itemCount,
              std::function<void(UIBuilder&, int index)> cellContent,
              float rowHeight = 0.0f);

    // --- Table/DataGrid ---
    void table(const std::string& id, std::vector<struct TableColumn>& columns,
               int rowCount, const Vec2& size,
               std::function<void(UIBuilder&, int row, int col)> cellContent,
               struct TableState* state = nullptr);

    // --- Image ---
    void image(const std::string& id, void* textureHandle, const Vec2& size,
               const Vec2& uv0 = {0,0}, const Vec2& uv1 = {1,1});

    // --- Dock Space (Phase 4) ---
    // Render a dock space that fills the available area
    void dockSpace(std::function<void(UIBuilder&)> content);
    // Render a docked panel — content only renders if panel is visible
    void dockPanel(const std::string& panelId, std::function<void(UIBuilder&)> content);

    // --- Layout helpers ---
    void spacing(float px);
    void sameLine(float offset = 0.0f);
    void setNextSize(float w, float h);
    void setNextConstraints(const LayoutConstraints& c);

    // --- Style overrides (Phase 6) ---
    void pushStyle(const Style& override);
    void popStyle();
    void pushButtonStyle(const ButtonStyle& s);
    void popButtonStyle();
    void pushPanelStyle(const PanelStyle& s);
    void popPanelStyle();
    void pushTextColor(const Color& color);
    void popTextColor();

    // --- Debug overlay (Phase C) ---
    void debugOverlay();

    // --- DPI helpers (Phase 4) ---
    float dpiScale() const;
    float scaled(float value) const;  // Returns value * dpiScale

    // --- Context access ---
    UIContext* context() { return ctx; }

private:
    UIContext* ctx;
};

} // namespace FluentUI
