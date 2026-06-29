// Brief 07 — data-driven Fluent material model (implementation).
#include "Theme/Material.h"
#include "Theme/FluentTheme.h"
#include <algorithm>

namespace FluentUI {

ColorTokens Tokens::Light() {
    ColorTokens t;
    t.accent          = FluentColors::Accent;
    t.accentHover     = FluentColors::AccentHover;
    t.accentPressed   = FluentColors::AccentPressed;
    t.surface         = FluentColors::Surface;
    t.surfaceAlt      = FluentColors::SurfaceAlt;
    t.surfaceElevated = FluentColors::SurfaceElevated;
    t.textPrimary     = FluentColors::TextPrimary;
    t.textSecondary   = FluentColors::TextSecondary;
    t.textTertiary    = FluentColors::TextTertiary;
    t.border          = FluentColors::Border;
    t.error           = FluentColors::Error;
    t.success         = FluentColors::Success;
    t.warning         = FluentColors::Warning;
    t.info            = FluentColors::Info;
    return t;
}

ColorTokens Tokens::Dark() {
    ColorTokens t;
    t.accent          = FluentColors::Accent;
    t.accentHover     = FluentColors::AccentHover;
    t.accentPressed   = FluentColors::AccentPressed;
    t.surface         = FluentColors::SurfaceDark;
    t.surfaceAlt      = FluentColors::SurfaceAltDark;
    t.surfaceElevated = FluentColors::SurfaceElevatedDark;
    t.textPrimary     = FluentColors::TextPrimaryDark;
    t.textSecondary   = FluentColors::TextSecondaryDark;
    t.textTertiary    = FluentColors::TextTertiaryDark;
    t.border          = FluentColors::BorderDark;
    t.error           = FluentColors::Error;
    t.success         = FluentColors::Success;
    t.warning         = FluentColors::Warning;
    t.info            = FluentColors::Info;
    return t;
}

ColorTokens Tokens::FromStyle(const Style& theme) {
    ColorTokens t = theme.isDarkTheme ? Dark() : Light();
    // Accent is whatever the active Style carries; derive hover/pressed exactly as
    // CreateCustomFluentStyle does, so the token palette tracks a custom accent.
    t.accent        = theme.accentColor;
    t.accentHover   = FluentColors::GetAccentHover(theme.accentColor);
    t.accentPressed = FluentColors::GetAccentPressed(theme.accentColor);
    return t;
}

// Pick the per-state color out of a ColorState (matches the widgets' getTargetColor).
static Color PickState(const ColorState& cs, WidgetState state) {
    switch (state) {
        case WidgetState::Disabled: return cs.disabled;
        case WidgetState::Pressed:  return cs.pressed;
        case WidgetState::Hover:    return cs.hover;
        case WidgetState::Rest:
        case WidgetState::Focused:
        default:                    return cs.normal;
    }
}

FluentMaterial ResolveButtonMaterial(const ButtonStyle& bs, WidgetState state) {
    FluentMaterial m;
    m.fill        = PickState(bs.background, state);
    m.border      = PickState(bs.border, state);
    m.borderWidth = bs.borderWidth;
    m.radius      = bs.cornerRadius;
    // State → z, identical to the previous inline mapping in Button:
    //   rest = ButtonRest(0), hover = ButtonHover, pressed = ButtonPressed, disabled = 0.
    m.elevationZ = (state == WidgetState::Disabled) ? 0.0f
                 : (state == WidgetState::Pressed)  ? Elevation::Z::ButtonPressed
                 : (state == WidgetState::Hover)    ? Elevation::Z::ButtonHover
                                                    : Elevation::Z::ButtonRest;
    // Reveal: enabled buttons react to the cursor (brief 04); disabled don't.
    m.revealIntensity = (state == WidgetState::Disabled) ? 0.0f : 1.0f;
    return m;
}

FluentMaterial ResolvePanelMaterial(const PanelStyle& ps, WidgetState state) {
    (void)state; // panels have a single resting appearance in the current Style
    FluentMaterial m;
    m.fill        = ps.background;
    m.border      = ps.borderColor;
    m.borderWidth = ps.borderWidth;
    m.radius      = ps.cornerRadius;
    m.elevationZ  = Elevation::Z::Card; // a card/panel floats at z=Card
    m.revealIntensity = 0.0f;
    m.acrylic     = ps.useAcrylic;
    m.tintOpacity = std::clamp(0.10f + 0.10f * ps.acrylicOpacity, 0.0f, 1.0f);
    m.luminosityOpacity = 0.85f;
    return m;
}

FluentMaterial ResolveMaterial(WidgetRole role, WidgetState state, const Style& theme) {
    switch (role) {
        case WidgetRole::ButtonPrimary:
        case WidgetRole::ButtonSecondary:
        case WidgetRole::MenuItem:
            return ResolveButtonMaterial(theme.button, state);
        case WidgetRole::Card:
        case WidgetRole::Surface:
        case WidgetRole::Input:
            return ResolvePanelMaterial(theme.panel, state);
        case WidgetRole::Flyout:
        case WidgetRole::Dialog: {
            FluentMaterial m = ResolvePanelMaterial(theme.panel, state);
            m.elevationZ = (role == WidgetRole::Dialog) ? Elevation::Z::Dialog
                                                        : Elevation::Z::Flyout;
            return m;
        }
        default:
            return ResolvePanelMaterial(theme.panel, state);
    }
}

SDFInstance MakeInstance(const Vec2& pos, const Vec2& size,
                         const FluentMaterial& m, float dpiScale) {
    SDFInstance s{};
    s.cx = pos.x + size.x * 0.5f;
    s.cy = pos.y + size.y * 0.5f;
    s.hx = size.x * 0.5f;
    s.hy = size.y * 0.5f;
    s.radius = std::clamp(m.radius, 0.0f, std::min(s.hx, s.hy));
    s.softness = std::max(1.0f, dpiScale);
    s.mode = 0.0f;
    s.fillR = m.fill.r; s.fillG = m.fill.g; s.fillB = m.fill.b; s.fillA = m.fill.a;
    if (m.borderWidth > 0.0f) {
        s.borderWidth = std::max(1.0f, dpiScale) * m.borderWidth;
        s.borderR = m.border.r; s.borderG = m.border.g; s.borderB = m.border.b; s.borderA = m.border.a;
    }
    s.revealIntensity = m.revealIntensity;
    return s;
}

} // namespace FluentUI
