#include "core/Demo.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "Theme/Style.h"
#include "Theme/FluentTheme.h"
#include "UI/Widgets.h"
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <cstdio>

namespace FluentUI {
namespace Demo {

void ShowStyleEditor(bool* open) {
    if (open && !*open) return;
    UIContext* ctx = GetContext();
    if (!ctx) return;

    Vec2 viewport = ctx->renderer.GetViewportSize();
    Vec2 panelSize(400.0f, 520.0f);
    Vec2 panelPos(20.0f, 20.0f);

    BeginPanel("FluentUIStyleEditor", panelSize, true,
               std::nullopt, std::nullopt, panelPos);
    BeginVertical(6.0f);

    Label("Style Editor", std::nullopt, TypographyStyle::Title);
    Separator();

    Style& s = ctx->style;

    // Theme mode toggle
    bool dark = s.isDarkTheme;
    if (Checkbox("Dark theme", &dark)) {
        s = dark ? GetDarkFluentStyle() : GetDefaultFluentStyle();
    }

    Separator();
    Label("Core colors", std::nullopt, TypographyStyle::Subtitle);
    ColorPicker("Background", &s.backgroundColor);
    ColorPicker("Accent", &s.accentColor);

    Separator();
    Label("Panel", std::nullopt, TypographyStyle::Subtitle);
    ColorPicker("Panel bg", &s.panel.background);
    ColorPicker("Panel header", &s.panel.headerBackground);
    ColorPicker("Panel border", &s.panel.borderColor);

    Separator();
    Label("Button", std::nullopt, TypographyStyle::Subtitle);
    ColorPicker("Btn normal", &s.button.background.normal);
    ColorPicker("Btn hover", &s.button.background.hover);
    ColorPicker("Btn pressed", &s.button.background.pressed);

    Separator();
    Label("Layout", std::nullopt, TypographyStyle::Subtitle);
    SliderFloat("Layout spacing", &s.spacing, 0.0f, 20.0f, 220.0f, "%.1f");

    EndVertical();
    EndPanel();
}

} // namespace Demo
} // namespace FluentUI
