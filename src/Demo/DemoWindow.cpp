#include "core/Demo.h"
#include "core/UIBuilder.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "UI/Widgets.h"
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace FluentUI {
namespace Demo {

namespace {

struct DemoState {
    int activeTab = 0;
    bool checkbox1 = true;
    bool checkbox2 = false;
    int radio = 0;
    float slider1 = 0.5f;
    int sliderInt = 50;
    float drag1 = 1.0f;
    int dragInt = 10;
    Color colorVal{0.4f, 0.6f, 0.9f, 1.0f};
    std::string textValue = "Edit me";
    int comboIdx = 0;
    int comboSearchIdx = 0;
    std::vector<std::string> comboItems = {
        "Apple", "Banana", "Cherry", "Date", "Elderberry",
        "Fig", "Grape", "Honeydew"
    };
    std::vector<std::string> listItems = {"Item A", "Item B", "Item C", "Item D", "Item E"};
    int listSel = -1;
    bool tree1Open = true;
    bool tree2Open = false;
    float plotData[64] = {};
    int plotOffset = 0;
    float plotPhase = 0.0f;
    DateTimeValue datetime{};
    bool linkClicked = false;
    std::string lastLinkUrl;
    std::vector<int> selectedRows;
    int frozenCols = 1;
};

DemoState* GetDemoState() {
    static DemoState s;
    return &s;
}

void PushPlotSample(DemoState* s, float v) {
    s->plotData[s->plotOffset] = v;
    s->plotOffset = (s->plotOffset + 1) % 64;
}

} // namespace

void ShowDemoWindow(bool* open) {
    if (open && !*open) return;
    DemoState* s = GetDemoState();

    UIContext* ctx = GetContext();
    if (!ctx) return;
    UIBuilder ui(ctx);

    // Generate live data for the plot widgets
    s->plotPhase += 0.07f;
    PushPlotSample(s, std::sin(s->plotPhase) * 0.5f + 0.5f);

    // Render at a fixed top-right anchor inside the main viewport for visibility
    Vec2 viewport = ctx->renderer.GetViewportSize();
    Vec2 panelSize(560.0f, 480.0f);
    Vec2 panelPos(viewport.x - panelSize.x - 20.0f, 20.0f);

    BeginPanel("FluentUIDemoWindow", panelSize, true, std::nullopt, std::nullopt, panelPos);
    BeginVertical(6.0f);

    Label("FluentUI — Demo Window", std::nullopt, TypographyStyle::Title);
    Separator();

    static const std::vector<std::string> tabs = {
        "Basic", "Input", "Lists", "Plots", "Date/Time", "Rich"
    };
    SegmentedControl("demo.tabs", tabs, &s->activeTab);

    Separator();

    if (s->activeTab == 0) {
        // BASIC widgets
        Label("Buttons & toggles", std::nullopt, TypographyStyle::Subtitle);
        ui.horizontal([&](UIBuilder& u) {
            (void)u.button("Primary");
            (void)u.button("Secondary");
            (void)u.button("Disabled", {0, 0}, false);
        });
        Checkbox("Checkbox 1", &s->checkbox1);
        Checkbox("Checkbox 2", &s->checkbox2);
        ui.horizontal([&](UIBuilder& u) {
            u.radioButton("Option A", &s->radio, 0, "demo.radio");
            u.radioButton("Option B", &s->radio, 1, "demo.radio");
            u.radioButton("Option C", &s->radio, 2, "demo.radio");
        });
        ProgressBar(s->slider1);
    } else if (s->activeTab == 1) {
        // INPUT widgets
        Label("Input controls", std::nullopt, TypographyStyle::Subtitle);
        SliderFloat("Float slider", &s->slider1, 0.0f, 1.0f, 220.0f, "%.2f");
        SliderInt("Int slider", &s->sliderInt, 0, 100, 220.0f);
        DragFloat("Drag float", &s->drag1, 0.05f, -10.0f, 10.0f);
        DragInt("Drag int", &s->dragInt, 1.0f, 0, 100);
        TextInput("Text", &s->textValue, 220.0f);
        ComboBox("Combo", &s->comboIdx, s->comboItems, 220.0f);
        ComboBoxSearchable("Searchable", &s->comboSearchIdx, s->comboItems, 220.0f);
        ColorPicker("Color", &s->colorVal);
    } else if (s->activeTab == 2) {
        // LISTS, TREES, TABLES
        Label("Lists & trees", std::nullopt, TypographyStyle::Subtitle);
        BeginListView("demo.listview", Vec2(220, 120), &s->listSel, s->listItems);
        EndListView();

        BeginTreeView("demo.treeview", Vec2(220, 120));
        if (TreeNode("demo.treeA", "Folder A", &s->tree1Open)) {
            Label("  child A1");
            Label("  child A2");
        }
        if (TreeNode("demo.treeB", "Folder B", &s->tree2Open)) {
            Label("  child B1");
        }
        EndTreeView();
    } else if (s->activeTab == 3) {
        // PLOTS
        Label("Plot widgets", std::nullopt, TypographyStyle::Subtitle);
        PlotLines("Sine", s->plotData, 64, s->plotOffset, "live", 0.0f, 1.0f, Vec2(0, 80));
        PlotHistogram("Hist", s->plotData, 64, s->plotOffset, "", 0.0f, 1.0f, Vec2(0, 80));
        ui.horizontal([&](UIBuilder& u) {
            u.label("Inline sparkline:");
            Sparkline(s->plotData, 64, Vec2(120, 18));
        });
    } else if (s->activeTab == 4) {
        // DATE / TIME
        Label("Date and time pickers", std::nullopt, TypographyStyle::Subtitle);
        DatePicker("Date", &s->datetime);
        TimePicker("Time", &s->datetime);
    } else if (s->activeTab == 5) {
        // RICH TEXT
        Label("Rich text inline markup", std::nullopt, TypographyStyle::Subtitle);
        LabelRich("Plain text with <b>bold</b> and <i>italic</i> tokens.");
        LabelRich("Color: <color=#ff5050>red</color>, <color=#50d050>green</color>, <color=#5080ff>blue</color>.");
        LabelRich("Sizes: <size=12>small</size> <size=18>medium</size> <size=24>large</size>.");
        LabelRich(
            "Click <a href=\"https://github.com\">this link</a> to fire the callback.",
            0.0f, std::nullopt, TypographyStyle::Body,
            [s](const std::string& url) {
                s->linkClicked = true;
                s->lastLinkUrl = url;
            });
        if (s->linkClicked) {
            Label("Last link: " + s->lastLinkUrl, std::nullopt, TypographyStyle::Caption);
        }
    }

    EndVertical();
    EndPanel();
}

void ShowAboutWindow(bool* open) {
    if (open && !*open) return;
    UIContext* ctx = GetContext();
    if (!ctx) return;

    Vec2 viewport = ctx->renderer.GetViewportSize();
    Vec2 panelSize(360.0f, 200.0f);
    Vec2 panelPos((viewport.x - panelSize.x) * 0.5f, (viewport.y - panelSize.y) * 0.5f);

    BeginPanel("FluentUIAboutWindow", panelSize, true, std::nullopt, std::nullopt, panelPos);
    BeginVertical(8.0f);
    Label("About FluentUI", std::nullopt, TypographyStyle::Title);
    Separator();
    Label("FluentUI — immediate-mode C++ GUI", std::nullopt, TypographyStyle::Body);
    Label("Phases A-H complete (paridad ImGui).", std::nullopt, TypographyStyle::Body);
    Label("Backend: OpenGL 3.3+ (Vulkan/DX11 deferred).", std::nullopt, TypographyStyle::Caption);
    Separator();
    if (open) {
        if (Button("Close")) *open = false;
    }
    EndVertical();
    EndPanel();
}

void ShowMetricsWindow(bool* open) {
    if (open && !*open) return;
    UIContext* ctx = GetContext();
    if (!ctx) return;

    Vec2 viewport = ctx->renderer.GetViewportSize();
    Vec2 panelSize(320.0f, 280.0f);
    Vec2 panelPos(20.0f, viewport.y - panelSize.y - 20.0f);

    BeginPanel("FluentUIMetricsWindow", panelSize, true, std::nullopt, std::nullopt, panelPos);
    BeginVertical(4.0f);
    Label("Metrics", std::nullopt, TypographyStyle::Title);
    Separator();

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Frame: %u", ctx->frame);
    Label(buf);
    std::snprintf(buf, sizeof(buf), "DPI scale: %.2f", ctx->dpiScale);
    Label(buf);
    std::snprintf(buf, sizeof(buf), "Active widget id: %u", ctx->activeWidgetId);
    Label(buf);
    std::snprintf(buf, sizeof(buf), "Focused widget id: %u", ctx->focusedWidgetId);
    Label(buf);
    Vec2 vp = ctx->renderer.GetViewportSize();
    std::snprintf(buf, sizeof(buf), "Viewport: %.0f x %.0f", vp.x, vp.y);
    Label(buf);

    EndVertical();
    EndPanel();
}

} // namespace Demo
} // namespace FluentUI
