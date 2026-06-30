// Brief 07 — data-driven Fluent material model (implementation).
#include "Theme/Material.h"
#include "Theme/FluentTheme.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

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

// ── Serialization ────────────────────────────────────────────────────────────
namespace {

// Color → "#RRGGBBAA". (Color::ToUint32 packs ABGR, so we format channels by hand.)
std::string ColorToHex(const Color& c) {
    auto byte = [](float v) {
        return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    char buf[10];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X",
                  byte(c.r), byte(c.g), byte(c.b), byte(c.a));
    return std::string(buf);
}

// Tolerant scalar finder: locate "key" and return the text after its ':' up to
// the next ',' or '}'. Returns false if the key is absent.
bool FindValue(const std::string& json, const char* key, std::string& out) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = json.find(needle);
    if (k == std::string::npos) return false;
    size_t colon = json.find(':', k + needle.size());
    if (colon == std::string::npos) return false;
    size_t i = colon + 1;
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t' ||
                               json[i] == '\n' || json[i] == '\r')) ++i;
    size_t end = i;
    while (end < json.size() && json[end] != ',' && json[end] != '}' &&
           json[end] != '\n' && json[end] != '\r') ++end;
    out = json.substr(i, end - i);
    // trim trailing whitespace
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
    return !out.empty();
}

void ParseFloat(const std::string& json, const char* key, float& dst) {
    std::string v;
    if (FindValue(json, key, v)) dst = std::strtof(v.c_str(), nullptr);
}

void ParseBool(const std::string& json, const char* key, bool& dst) {
    std::string v;
    if (FindValue(json, key, v)) dst = (v.find("true") != std::string::npos);
}

void ParseColor(const std::string& json, const char* key, Color& dst) {
    std::string v;
    if (!FindValue(json, key, v)) return;
    size_t q1 = v.find('"');
    size_t q2 = (q1 == std::string::npos) ? std::string::npos : v.find('"', q1 + 1);
    if (q1 != std::string::npos && q2 != std::string::npos) {
        dst = Color::FromHex(v.substr(q1, q2 - q1 + 1).c_str());
    }
}

} // namespace

std::string MaterialToJson(const FluentMaterial& m) {
    std::ostringstream os;
    os << "{\n";
    os << "  \"fill\": \""              << ColorToHex(m.fill)   << "\",\n";
    os << "  \"border\": \""            << ColorToHex(m.border) << "\",\n";
    os << "  \"borderWidth\": "         << m.borderWidth        << ",\n";
    os << "  \"radius\": "              << m.radius             << ",\n";
    os << "  \"elevationZ\": "          << m.elevationZ         << ",\n";
    os << "  \"revealIntensity\": "     << m.revealIntensity    << ",\n";
    os << "  \"acrylic\": "             << (m.acrylic ? "true" : "false") << ",\n";
    os << "  \"tintOpacity\": "         << m.tintOpacity        << ",\n";
    os << "  \"luminosityOpacity\": "   << m.luminosityOpacity  << "\n";
    os << "}\n";
    return os.str();
}

FluentMaterial MaterialFromJson(const std::string& json) {
    FluentMaterial m; // start from defaults; absent keys keep them
    ParseColor(json, "fill",   m.fill);
    ParseColor(json, "border", m.border);
    ParseFloat(json, "borderWidth",       m.borderWidth);
    ParseFloat(json, "radius",            m.radius);
    ParseFloat(json, "elevationZ",        m.elevationZ);
    ParseFloat(json, "revealIntensity",   m.revealIntensity);
    ParseBool (json, "acrylic",           m.acrylic);
    ParseFloat(json, "tintOpacity",       m.tintOpacity);
    ParseFloat(json, "luminosityOpacity", m.luminosityOpacity);
    return m;
}

bool SaveMaterial(const std::string& filepath, const FluentMaterial& m) {
    std::ofstream f(filepath, std::ios::binary);
    if (!f) return false;
    f << MaterialToJson(m);
    return static_cast<bool>(f);
}

bool LoadMaterial(const std::string& filepath, FluentMaterial& out) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = MaterialFromJson(ss.str());
    return true;
}

} // namespace FluentUI
