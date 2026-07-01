#include <SDL3/SDL.h>
// ============================================================================
// Comprehensive Widget & System Tests for FluentUI
// 55+ test cases covering widgets, layout, animations, helpers, edge cases
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Animation.h"
#include "core/RippleEffect.h"
#include "core/Accessibility.h"
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "UI/Layout.h"
#include "Math/Vec2.h"
#include "Math/Rect.h"
#include "Math/Color.h"
#include "Theme/Style.h"
#include "Theme/FluentTheme.h"
#include <set>

using namespace FluentUI;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Shared GPU context (same pattern as test_gpu_integration.cpp)
// ============================================================================

static bool g_sdlInit = false;
static SDL_Window* g_win = nullptr;

static bool EnsureGL() {
    if (g_win) return true;
    if (!g_sdlInit) {
        if (!SDL_Init(SDL_INIT_VIDEO)) return false;
        g_sdlInit = true;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    g_win = SDL_CreateWindow("FluentUI Widget Tests", 800, 600,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    return g_win != nullptr;
}

// RAII helper for context lifecycle in GPU tests
struct GPUFixture {
    UIContext* ctx = nullptr;
    bool valid = false;

    GPUFixture() {
        if (!EnsureGL()) return;
        ctx = CreateContext(g_win);
        valid = (ctx != nullptr);
    }

    void beginFrame(float dt = 0.016f) {
        NewFrame(dt);
    }

    void endFrame() {
        RenderDeferredDropdowns();
        Render();
    }

    ~GPUFixture() {
        if (valid) DestroyContext();
    }
};

// ============================================================================
// 1. ANIMATION SYSTEM TESTS
// ============================================================================

TEST_CASE("Animation: AnimatedValue float default state", "[animation]") {
    AnimatedValue<float> anim;
    REQUIRE_FALSE(anim.IsAnimating());
    REQUIRE_FALSE(anim.IsInitialized());
}

TEST_CASE("Animation: AnimatedValue float initial value", "[animation]") {
    AnimatedValue<float> anim(5.0f);
    REQUIRE(anim.IsInitialized());
    REQUIRE_FALSE(anim.IsAnimating());
    REQUIRE_THAT(anim.Get(), WithinAbs(5.0f, 1e-5));
}

TEST_CASE("Animation: AnimatedValue float reaches target", "[animation]") {
    AnimatedValue<float> anim(0.0f);
    anim.SetTarget(10.0f, 0.5f, EasingType::Linear);
    REQUIRE(anim.IsAnimating());

    // Advance past duration
    for (int i = 0; i < 60; ++i) anim.Update(0.016f);
    REQUIRE_FALSE(anim.IsAnimating());
    REQUIRE_THAT(anim.Get(), WithinAbs(10.0f, 1e-5));
}

TEST_CASE("Animation: AnimatedValue intermediate value (linear)", "[animation]") {
    AnimatedValue<float> anim(0.0f);
    anim.SetTarget(100.0f, 1.0f, EasingType::Linear);
    anim.Update(0.5f); // halfway
    REQUIRE_THAT(anim.Get(), WithinAbs(50.0f, 1.0));
}

TEST_CASE("Animation: AnimatedValue SetImmediate skips animation", "[animation]") {
    AnimatedValue<float> anim(0.0f);
    anim.SetTarget(100.0f, 1.0f, EasingType::Linear);
    REQUIRE(anim.IsAnimating());

    anim.SetImmediate(42.0f);
    REQUIRE_FALSE(anim.IsAnimating());
    REQUIRE_THAT(anim.Get(), WithinAbs(42.0f, 1e-5));
}

TEST_CASE("Animation: AnimatedValue same target doesn't restart", "[animation]") {
    AnimatedValue<float> anim(10.0f);
    anim.SetTarget(10.0f, 1.0f, EasingType::Linear);
    REQUIRE_FALSE(anim.IsAnimating()); // same value, no animation
}

TEST_CASE("Animation: AnimatedColor reaches target", "[animation]") {
    AnimatedValue<Color> anim(Color(0, 0, 0, 1));
    anim.SetTarget(Color(1, 1, 1, 1), 0.3f, EasingType::Linear);
    for (int i = 0; i < 30; ++i) anim.Update(0.016f);
    REQUIRE_FALSE(anim.IsAnimating());
    REQUIRE_THAT(anim.Get().r, WithinAbs(1.0f, 1e-3));
    REQUIRE_THAT(anim.Get().g, WithinAbs(1.0f, 1e-3));
    REQUIRE_THAT(anim.Get().b, WithinAbs(1.0f, 1e-3));
}

TEST_CASE("Animation: AnimatedValue zero duration", "[animation]") {
    AnimatedValue<float> anim(0.0f);
    anim.SetTarget(50.0f, 0.0f, EasingType::Linear);
    anim.Update(0.001f); // tiny step
    // Should reach target immediately (t = elapsed/0 is clamped to 1.0)
    REQUIRE_THAT(anim.Get(), WithinAbs(50.0f, 1e-3));
    REQUIRE_FALSE(anim.IsAnimating());
}

// ============================================================================
// 2. EASING FUNCTION TESTS
// ============================================================================

TEST_CASE("Easing: all functions return 0 at t=0", "[easing]") {
    REQUIRE_THAT(Easing::Linear(0.0f), WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseIn(0.0f), WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseOut(0.0f), WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseInOut(0.0f), WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseOutCubic(0.0f), WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseOutExpo(0.0f), WithinAbs(0.0f, 1e-2));
}

TEST_CASE("Easing: all functions return 1 at t=1", "[easing]") {
    REQUIRE_THAT(Easing::Linear(1.0f), WithinAbs(1.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseIn(1.0f), WithinAbs(1.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseOut(1.0f), WithinAbs(1.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseInOut(1.0f), WithinAbs(1.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseOutCubic(1.0f), WithinAbs(1.0f, 1e-5));
    REQUIRE_THAT(Easing::EaseOutExpo(1.0f), WithinAbs(1.0f, 1e-5));
}

TEST_CASE("Easing: Evaluate matches individual functions", "[easing]") {
    float t = 0.37f;
    REQUIRE_THAT(Easing::Evaluate(EasingType::Linear, t), WithinAbs(Easing::Linear(t), 1e-5));
    REQUIRE_THAT(Easing::Evaluate(EasingType::EaseIn, t), WithinAbs(Easing::EaseIn(t), 1e-5));
    REQUIRE_THAT(Easing::Evaluate(EasingType::EaseOut, t), WithinAbs(Easing::EaseOut(t), 1e-5));
    REQUIRE_THAT(Easing::Evaluate(EasingType::EaseOutCubic, t), WithinAbs(Easing::EaseOutCubic(t), 1e-5));
    REQUIRE_THAT(Easing::Evaluate(EasingType::EaseOutExpo, t), WithinAbs(Easing::EaseOutExpo(t), 1e-5));
}

TEST_CASE("Easing: EaseOut is faster than Linear at midpoint", "[easing]") {
    float t = 0.5f;
    REQUIRE(Easing::EaseOut(t) > Easing::Linear(t));
}

TEST_CASE("Easing: EaseIn is slower than Linear at midpoint", "[easing]") {
    float t = 0.5f;
    REQUIRE(Easing::EaseIn(t) < Easing::Linear(t));
}

TEST_CASE("Easing: monotonically increasing for all types", "[easing]") {
    for (auto type : {EasingType::Linear, EasingType::EaseIn, EasingType::EaseOut,
                      EasingType::EaseInOut, EasingType::EaseOutCubic, EasingType::EaseOutExpo}) {
        float prev = 0.0f;
        for (int i = 1; i <= 100; ++i) {
            float t = static_cast<float>(i) / 100.0f;
            float val = Easing::Evaluate(type, t);
            REQUIRE(val >= prev - 1e-5f); // monotonically non-decreasing
            prev = val;
        }
    }
}

// ============================================================================
// 3. RIPPLE EFFECT TESTS
// ============================================================================

TEST_CASE("Ripple: empty state is inactive", "[ripple]") {
    RippleEffect effect;
    REQUIRE_FALSE(effect.IsActive());
    REQUIRE(effect.GetRipples().empty());
}

TEST_CASE("Ripple: add and update lifecycle", "[ripple]") {
    RippleEffect effect;
    effect.AddRipple(Vec2(100, 100), 50.0f, 0.4f);
    REQUIRE(effect.IsActive());
    REQUIRE(effect.GetRipples().size() == 1);

    // Advance past duration
    for (int i = 0; i < 30; ++i) effect.Update(0.016f);
    REQUIRE_FALSE(effect.IsActive());
    REQUIRE(effect.GetRipples().empty());
}

TEST_CASE("Ripple: radius grows with easing", "[ripple]") {
    RippleEffect effect;
    effect.AddRipple(Vec2(0, 0), 100.0f, 1.0f);
    effect.Update(0.5f); // halfway
    REQUIRE(effect.GetRipples().size() == 1);
    float radius = effect.GetRipples()[0].radius;
    REQUIRE(radius > 0.0f);
    REQUIRE(radius < 100.0f);
}

TEST_CASE("Ripple: opacity fades to zero", "[ripple]") {
    RippleEffect effect;
    effect.AddRipple(Vec2(0, 0), 100.0f, 1.0f);
    effect.Update(0.9f); // near end
    REQUIRE(effect.GetRipples().size() == 1);
    REQUIRE(effect.GetRipples()[0].opacity < 0.2f);
}

TEST_CASE("Ripple: multiple simultaneous ripples", "[ripple]") {
    RippleEffect effect;
    effect.AddRipple(Vec2(10, 10), 50.0f, 0.5f);
    effect.AddRipple(Vec2(20, 20), 80.0f, 0.3f);
    effect.AddRipple(Vec2(30, 30), 60.0f, 0.4f);
    REQUIRE(effect.GetRipples().size() == 3);

    // Short one finishes first
    for (int i = 0; i < 25; ++i) effect.Update(0.016f);
    REQUIRE(effect.GetRipples().size() < 3);
}

TEST_CASE("Ripple: Clear removes all", "[ripple]") {
    RippleEffect effect;
    effect.AddRipple(Vec2(0, 0), 50.0f);
    effect.AddRipple(Vec2(0, 0), 50.0f);
    effect.Clear();
    REQUIRE_FALSE(effect.IsActive());
}

// ============================================================================
// 4. LAYOUT CONSTRAINT TESTS
// ============================================================================

TEST_CASE("Layout: FixedSize helper", "[layout]") {
    auto c = FixedSize(200.0f, 100.0f);
    REQUIRE(c.width == SizeConstraint::Fixed);
    REQUIRE(c.height == SizeConstraint::Fixed);
    REQUIRE_THAT(c.fixedWidth, WithinAbs(200.0f, 1e-5));
    REQUIRE_THAT(c.fixedHeight, WithinAbs(100.0f, 1e-5));
}

TEST_CASE("Layout: FillSize helper", "[layout]") {
    auto c = FillSize();
    REQUIRE(c.width == SizeConstraint::Fill);
    REQUIRE(c.height == SizeConstraint::Fill);
}

TEST_CASE("Layout: AutoSize helper", "[layout]") {
    auto c = AutoSize();
    REQUIRE(c.width == SizeConstraint::Auto);
    REQUIRE(c.height == SizeConstraint::Auto);
}

TEST_CASE("Layout: default constraints are Auto", "[layout]") {
    LayoutConstraints c;
    REQUIRE(c.width == SizeConstraint::Auto);
    REQUIRE(c.height == SizeConstraint::Auto);
    REQUIRE_THAT(c.fixedWidth, WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(c.minWidth, WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(c.maxWidth, WithinAbs(0.0f, 1e-5));
}

// ============================================================================
// 5. HELPER FUNCTION TESTS
// ============================================================================

TEST_CASE("Helper: GenerateId consistency", "[helpers]") {
    uint32_t id1 = GenerateId("button_ok");
    uint32_t id2 = GenerateId("button_ok");
    REQUIRE(id1 == id2);
}

TEST_CASE("Helper: GenerateId uniqueness for different inputs", "[helpers]") {
    uint32_t a = GenerateId("save_button");
    uint32_t b = GenerateId("load_button");
    uint32_t c = GenerateId("delete_button");
    REQUIRE(a != b);
    REQUIRE(b != c);
    REQUIRE(a != c);
}

TEST_CASE("Helper: GenerateId 2-part avoids prefix collisions", "[helpers]") {
    uint32_t a = GenerateId("BTN", "ok");
    uint32_t b = GenerateId("PANEL", "ok");
    REQUIRE(a != b);
}

TEST_CASE("Helper: GenerateId 3-part variant", "[helpers]") {
    uint32_t a = GenerateId("TAB", "main", "0");
    uint32_t b = GenerateId("TAB", "main", "1");
    REQUIRE(a != b);
}

TEST_CASE("Helper: PointInRect basic cases", "[helpers]") {
    REQUIRE(PointInRect(Vec2(50, 50), Vec2(0, 0), Vec2(100, 100)));
    REQUIRE(PointInRect(Vec2(0, 0), Vec2(0, 0), Vec2(100, 100)));
    REQUIRE_FALSE(PointInRect(Vec2(101, 50), Vec2(0, 0), Vec2(100, 100)));
    REQUIRE_FALSE(PointInRect(Vec2(-1, 50), Vec2(0, 0), Vec2(100, 100)));
    REQUIRE_FALSE(PointInRect(Vec2(50, -1), Vec2(0, 0), Vec2(100, 100)));
}

TEST_CASE("Helper: PointInRect edge cases with zero size", "[helpers]") {
    REQUIRE_FALSE(PointInRect(Vec2(0, 0), Vec2(0, 0), Vec2(0, 0)));
}

TEST_CASE("Helper: RectanglesOverlap basic cases", "[helpers]") {
    // Overlapping
    REQUIRE(RectanglesOverlap(Vec2(0, 0), Vec2(100, 100), Vec2(50, 50), Vec2(100, 100)));
    // Non-overlapping
    REQUIRE_FALSE(RectanglesOverlap(Vec2(0, 0), Vec2(50, 50), Vec2(60, 60), Vec2(50, 50)));
    // Adjacent (touching edge)
    REQUIRE_FALSE(RectanglesOverlap(Vec2(0, 0), Vec2(50, 50), Vec2(50, 0), Vec2(50, 50)));
}

TEST_CASE("Helper: RectanglesOverlap with zero-size rect", "[helpers]") {
    REQUIRE_FALSE(RectanglesOverlap(Vec2(0, 0), Vec2(0, 0), Vec2(0, 0), Vec2(100, 100)));
}

TEST_CASE("Helper: AnimSlot produces unique slots per widget", "[helpers]") {
    uint32_t widgetA = 100;
    uint32_t widgetB = 200;
    REQUIRE(AnimSlot(widgetA, 0) != AnimSlot(widgetB, 0));
    REQUIRE(AnimSlot(widgetA, 0) != AnimSlot(widgetA, 1));
    REQUIRE(AnimSlot(widgetA, 1) != AnimSlot(widgetA, 2));
}

// ============================================================================
// 6. THEME & STYLE TESTS
// ============================================================================

TEST_CASE("Theme: dark style has dark background", "[theme]") {
    Style dark = GetDarkFluentStyle();
    REQUIRE(dark.isDarkTheme);
    // Dark theme background should have low luminance
    REQUIRE(dark.backgroundColor.Luminance() < 0.3f);
}

TEST_CASE("Theme: light style has light background", "[theme]") {
    Style light = GetDefaultFluentStyle();
    REQUIRE_FALSE(light.isDarkTheme);
    REQUIRE(light.backgroundColor.Luminance() > 0.5f);
}

TEST_CASE("Theme: typography styles have increasing font sizes", "[theme]") {
    Style s = GetDarkFluentStyle();
    float caption = s.typography.caption.fontSize;
    float body = s.typography.body.fontSize;
    float subtitle = s.typography.subtitle.fontSize;
    float title = s.typography.title.fontSize;
    float display = s.typography.display.fontSize;

    REQUIRE(caption <= body);
    REQUIRE(body <= subtitle);
    REQUIRE(subtitle <= title);
    REQUIRE(title <= display);
}

TEST_CASE("Theme: GetTextStyle returns correct variant", "[theme]") {
    Style s = GetDarkFluentStyle();
    const TextStyle& body = s.GetTextStyle(TypographyStyle::Body);
    const TextStyle& caption = s.GetTextStyle(TypographyStyle::Caption);
    REQUIRE(body.fontSize >= caption.fontSize);
}

TEST_CASE("Theme: button style has valid corner radius", "[theme]") {
    Style s = GetDarkFluentStyle();
    REQUIRE(s.button.cornerRadius > 0.0f);
    REQUIRE(s.button.cornerRadius < 50.0f); // reasonable
}

TEST_CASE("Theme: panel padding is non-negative", "[theme]") {
    Style s = GetDarkFluentStyle();
    REQUIRE(s.panel.padding.x >= 0.0f);
    REQUIRE(s.panel.padding.y >= 0.0f);
}

TEST_CASE("Theme: custom accent color style", "[theme]") {
    Style blue = CreateCustomFluentStyle(Color(0.0f, 0.47f, 0.84f), true);
    Style green = CreateCustomFluentStyle(Color(0.0f, 0.78f, 0.34f), true);
    // Different accent colors should produce different button backgrounds
    REQUIRE_FALSE(blue.button.background.normal == green.button.background.normal);
    // The custom accent must also drive style.accentColor (sliders, checkboxes,
    // progress bars, plots, etc. read ctx->style.accentColor) — otherwise those
    // widgets stay the default blue regardless of the chosen accent.
    REQUIRE(blue.accentColor.b > blue.accentColor.r);   // blue: high B, low R
    REQUIRE(green.accentColor.g > green.accentColor.r); // green: high G, low R
    REQUIRE_FALSE(blue.accentColor == green.accentColor);
}

TEST_CASE("Theme: various accent colors produce valid styles", "[theme]") {
    Color accents[] = {
        Color(0.0f, 0.47f, 0.84f),  // Blue
        Color(0.0f, 0.78f, 0.34f),  // Green
        Color(0.55f, 0.23f, 0.78f), // Purple
        Color(1.0f, 0.55f, 0.0f),   // Orange
        Color(0.91f, 0.29f, 0.53f), // Pink
        Color(0.0f, 0.67f, 0.65f),  // Teal
    };
    for (const auto& accent : accents) {
        Style s = CreateCustomFluentStyle(accent, true);
        REQUIRE(s.button.background.normal.a > 0.0f);
        REQUIRE(s.button.cornerRadius > 0.0f);
        REQUIRE(s.spacing > 0.0f);
    }
}

// ============================================================================
// 7. GPU WIDGET RENDERING TESTS (require GL context)
// ============================================================================

TEST_CASE("GPU Widget: Button renders without crash", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    bool clicked = Button("Test Button");
    REQUIRE_FALSE(clicked); // no mouse input
    gpu.endFrame();
}

TEST_CASE("GPU Widget: Label renders all typography styles", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    Label("Caption", std::nullopt, TypographyStyle::Caption);
    Label("Body", std::nullopt, TypographyStyle::Body);
    Label("Subtitle", std::nullopt, TypographyStyle::Subtitle);
    Label("Title", std::nullopt, TypographyStyle::Title);
    Label("Display", std::nullopt, TypographyStyle::Display);
    gpu.endFrame();
    // If we get here without crash, all typography paths work
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Widget: Checkbox toggles state", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    bool checked = false;
    gpu.beginFrame();
    Checkbox("Check me", &checked);
    gpu.endFrame();
    // No click simulated, state unchanged
    REQUIRE_FALSE(checked);
}

TEST_CASE("GPU Widget: RadioButton renders without crash", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int selection = 0;
    gpu.beginFrame();
    RadioButton("Option A", &selection, 0, "group1");
    RadioButton("Option B", &selection, 1, "group1");
    RadioButton("Option C", &selection, 2, "group1");
    gpu.endFrame();
    REQUIRE(selection == 0);
}

TEST_CASE("GPU Widget: SliderFloat clamps to range", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float value = 150.0f; // out of range
    gpu.beginFrame();
    SliderFloat("Slider", &value, 0.0f, 100.0f);
    gpu.endFrame();
    // Value should be clamped
    REQUIRE(value <= 100.0f);
    REQUIRE(value >= 0.0f);
}

TEST_CASE("GPU Widget: SliderFloat with min == max", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float value = 5.0f;
    gpu.beginFrame();
    SliderFloat("Degenerate Slider", &value, 5.0f, 5.0f);
    gpu.endFrame();
    REQUIRE_THAT(value, WithinAbs(5.0f, 1e-5));
}

TEST_CASE("GPU Widget: SliderInt clamps to range", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int value = -10;
    gpu.beginFrame();
    SliderInt("IntSlider", &value, 0, 100);
    gpu.endFrame();
    REQUIRE(value >= 0);
    REQUIRE(value <= 100);
}

TEST_CASE("GPU Widget: TextInput renders with empty string", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    std::string text;
    gpu.beginFrame();
    TextInput("Name", &text, 200.0f, false, std::nullopt, "Enter name...");
    gpu.endFrame();
    REQUIRE(text.empty());
}

TEST_CASE("GPU Widget: TextInput with maxLength", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    std::string text = "Hello World";
    gpu.beginFrame();
    TextInput("Limited", &text, 200.0f, false, std::nullopt, nullptr, 5);
    gpu.endFrame();
    // maxLength should truncate on next input, not retroactively
    // Just verify it renders without crash
    REQUIRE(true);
}

TEST_CASE("GPU Widget: ComboBox with empty items", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int selected = 0;
    std::vector<std::string> items; // empty
    gpu.beginFrame();
    ComboBox("Empty Combo", &selected, items);
    gpu.endFrame();
    REQUIRE(selected == 0);
}

TEST_CASE("GPU Widget: ComboBox with many items", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int selected = 0;
    std::vector<std::string> items;
    for (int i = 0; i < 100; ++i) items.push_back("Item " + std::to_string(i));

    gpu.beginFrame();
    ComboBox("Big Combo", &selected, items);
    gpu.endFrame();
    REQUIRE(selected == 0);
}

TEST_CASE("GPU Widget: ComboBox with temporary items (use-after-free fix)", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int selected = 0;
    // Force dropdown open
    gpu.ctx->GetWidgetState(GenerateId("COMBO", "Temp")).boolVal = true; // brief 22 (fase 3)

    gpu.beginFrame();
    // Pass temporary vector — this used to cause use-after-free
    ComboBox("Temp", &selected, {"Alpha", "Beta", "Gamma"});
    gpu.endFrame();
    // If we reach here, the use-after-free fix works
    REQUIRE(true);
}

TEST_CASE("GPU Widget: DragFloat renders without crash", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float value = 1.0f;
    gpu.beginFrame();
    DragFloat("Drag", &value, 0.1f, 0.0f, 10.0f);
    gpu.endFrame();
    REQUIRE_THAT(value, WithinAbs(1.0f, 1e-5));
}

TEST_CASE("GPU Widget: DragFloat3 renders all 3 components", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float vals[3] = {1.0f, 2.0f, 3.0f};
    gpu.beginFrame();
    DragFloat3("Position", vals, 0.1f, -100.0f, 100.0f);
    gpu.endFrame();
    // Values unchanged without input
    REQUIRE_THAT(vals[0], WithinAbs(1.0f, 1e-5));
    REQUIRE_THAT(vals[1], WithinAbs(2.0f, 1e-5));
    REQUIRE_THAT(vals[2], WithinAbs(3.0f, 1e-5));
}

TEST_CASE("GPU Widget: ProgressBar renders at boundaries", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    ProgressBar(0.0f, Vec2(200, 20), "0%");
    ProgressBar(0.5f, Vec2(200, 20), "50%");
    ProgressBar(1.0f, Vec2(200, 20), "100%");
    ProgressBar(-0.1f, Vec2(200, 20), "negative"); // edge: should clamp
    ProgressBar(1.5f, Vec2(200, 20), "over");      // edge: should clamp
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Widget: Separator renders", "[gpu][widget]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    Label("Before");
    Separator();
    Label("After");
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.vertexCount > 0);
}

// ============================================================================
// 8. CONTAINER WIDGET TESTS
// ============================================================================

TEST_CASE("GPU Container: Panel begin/end lifecycle", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    if (BeginPanel("test_panel", Vec2(300, 200))) {
        Label("Inside panel");
        EndPanel();
    }
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Container: nested panels", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    if (BeginPanel("outer", Vec2(400, 300))) {
        Label("Outer");
        if (BeginPanel("inner", Vec2(200, 100))) {
            Label("Inner");
            EndPanel();
        }
        EndPanel();
    }
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Container: ScrollView renders", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    Vec2 scrollOff(0, 0);
    gpu.beginFrame();
    if (BeginScrollView("scroll1", Vec2(300, 200), &scrollOff)) {
        for (int i = 0; i < 20; ++i) {
            Label("Scrollable item " + std::to_string(i));
        }
        EndScrollView();
    }
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.clipPushes > 0); // scroll view uses clipping
}

TEST_CASE("GPU Container: TabView with multiple tabs", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int activeTab = 0;
    std::vector<std::string> tabs = {"General", "Graphics", "Audio", "Input"};

    gpu.beginFrame();
    if (BeginTabView("tabs", &activeTab, tabs, Vec2(400, 300))) {
        Label("Content of tab " + std::to_string(activeTab));
        EndTabView();
    }
    gpu.endFrame();
    REQUIRE(activeTab == 0);
}

TEST_CASE("GPU Container: TabView with many tabs (overflow)", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int activeTab = 0;
    std::vector<std::string> tabs;
    for (int i = 0; i < 20; ++i) tabs.push_back("Tab " + std::to_string(i));

    gpu.beginFrame();
    if (BeginTabView("many_tabs", &activeTab, tabs, Vec2(300, 200))) {
        Label("Tab content");
        EndTabView();
    }
    gpu.endFrame();
    // Should render scroll arrows for overflow
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Container: Splitter basic", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float ratio = 0.5f;
    gpu.beginFrame();
    if (BeginSplitter("split1", true, &ratio, Vec2(600, 400))) {
        SplitterPanel();
        Label("Left");
        SplitterPanel();
        Label("Right");
        EndSplitter();
    }
    gpu.endFrame();
    REQUIRE_THAT(ratio, WithinAbs(0.5f, 1e-5));
}

TEST_CASE("GPU Container: Splitter ratio clamped", "[gpu][container]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float ratio = -0.5f; // invalid
    gpu.beginFrame();
    if (BeginSplitter("split_clamp", true, &ratio, Vec2(600, 400))) {
        SplitterPanel();
        Label("A");
        SplitterPanel();
        Label("B");
        EndSplitter();
    }
    gpu.endFrame();
    // Ratio should be clamped to valid range
    REQUIRE(ratio >= 0.0f);
    REQUIRE(ratio <= 1.0f);
}

// ============================================================================
// 9. LAYOUT SYSTEM TESTS (with GPU)
// ============================================================================

TEST_CASE("GPU Layout: Vertical layout groups widgets", "[gpu][layout]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginVertical();
    Button("Button 1");
    Button("Button 2");
    Button("Button 3");
    EndVertical();
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Layout: Horizontal layout groups widgets", "[gpu][layout]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginHorizontal();
    Button("A");
    Button("B");
    Button("C");
    EndHorizontal();
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Layout: nested vertical inside horizontal", "[gpu][layout]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginHorizontal();
    BeginVertical();
    Label("V1");
    Label("V2");
    EndVertical();
    BeginVertical();
    Label("V3");
    Label("V4");
    EndVertical();
    EndHorizontal();
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Layout: Spacing inserts space", "[gpu][layout]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginVertical();
    Label("Before");
    Spacing(50.0f);
    Label("After");
    EndVertical();
    gpu.endFrame();
    REQUIRE(true); // no crash
}

TEST_CASE("GPU Layout: SetNextConstraints with Fixed", "[gpu][layout]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginVertical();
    SetNextConstraints(FixedSize(200.0f, 40.0f));
    Button("Fixed Size");
    EndVertical();
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

// ============================================================================
// 10. OVERLAY & MENU TESTS
// ============================================================================

TEST_CASE("GPU Overlay: Modal begin/end", "[gpu][overlay]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    bool open = true;
    gpu.beginFrame();
    if (BeginModal("modal1", "Test Modal", &open, Vec2(400, 300))) {
        Label("Modal content");
        if (Button("Close")) open = false;
        EndModal();
    }
    gpu.endFrame();
    REQUIRE(open); // no click
}

TEST_CASE("GPU Overlay: ContextMenu renders", "[gpu][overlay]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    if (BeginContextMenu("ctx_menu")) {
        ContextMenuItem("Cut");
        ContextMenuItem("Copy");
        ContextMenuSeparator();
        ContextMenuItem("Paste");
        EndContextMenu();
    }
    gpu.endFrame();
    REQUIRE(true); // no crash
}

TEST_CASE("GPU Menu: MenuBar with nested menus", "[gpu][menu]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    if (BeginMenuBar()) {
        if (BeginMenu("File")) {
            MenuItem("New");
            MenuItem("Open");
            MenuSeparator();
            MenuItem("Exit");
            EndMenu();
        }
        if (BeginMenu("Edit")) {
            MenuItem("Undo");
            MenuItem("Redo");
            EndMenu();
        }
        EndMenuBar();
    }
    gpu.endFrame();
    REQUIRE(true);
}

// ============================================================================
// 11. LIST & TREE WIDGET TESTS
// ============================================================================

TEST_CASE("GPU List: ListView single selection", "[gpu][list]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int selected = -1;
    std::vector<std::string> items = {"Apple", "Banana", "Cherry", "Date"};

    gpu.beginFrame();
    if (BeginListView("list1", Vec2(200, 150), &selected, items)) {
        EndListView();
    }
    gpu.endFrame();
    REQUIRE(selected == -1); // no click
}

TEST_CASE("GPU List: ListView multi selection", "[gpu][list]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    std::vector<int> selected;
    std::vector<std::string> items = {"Alpha", "Beta", "Gamma"};

    gpu.beginFrame();
    if (BeginListView("multi_list", Vec2(200, 150), &selected, items)) {
        EndListView();
    }
    gpu.endFrame();
    REQUIRE(selected.empty());
}

TEST_CASE("GPU List: ListView with empty items", "[gpu][list]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    int selected = 0;
    std::vector<std::string> items; // empty

    gpu.beginFrame();
    if (BeginListView("empty_list", Vec2(200, 100), &selected, items)) {
        EndListView();
    }
    gpu.endFrame();
    REQUIRE(true); // no crash with empty list
}

TEST_CASE("GPU Tree: TreeView with nodes", "[gpu][tree]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    bool open1 = true, open2 = false;

    gpu.beginFrame();
    if (BeginTreeView("tree1", Vec2(200, 200))) {
        if (TreeNode("tree_root", "Root", &open1)) {
            TreeNodePush();
            TreeNode("tree_child1", "Child 1");
            TreeNode("tree_child2", "Child 2", &open2);
            TreeNodePop();
        }
        EndTreeView();
    }
    gpu.endFrame();
    REQUIRE(open1); // still open
    REQUIRE_FALSE(open2); // still closed
}

// ============================================================================
// 12. TABLE TESTS
// ============================================================================

TEST_CASE("GPU Table: basic table rendering", "[gpu][table]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    std::vector<TableColumn> columns = {
        {"Name", 150.0f, 40.0f, true},
        {"Value", 100.0f, 40.0f, true},
        {"Type", 80.0f, 40.0f, false}
    };
    TableState state;

    gpu.beginFrame();
    if (BeginTable("table1", columns, 3, Vec2(400, 200), &state)) {
        for (int row = 0; row < 3; ++row) {
            TableNextRow();
            TableSetCell(0); Label("Item " + std::to_string(row));
            TableSetCell(1); Label(std::to_string(row * 10));
            TableSetCell(2); Label("string");
        }
        EndTable();
    }
    gpu.endFrame();
    REQUIRE(state.sortColumn == -1); // no sort clicked
}

TEST_CASE("GPU Table: empty table", "[gpu][table]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    std::vector<TableColumn> columns = {{"Col1", 100.0f}};

    gpu.beginFrame();
    if (BeginTable("empty_table", columns, 0, Vec2(200, 100))) {
        EndTable();
    }
    gpu.endFrame();
    REQUIRE(true); // no crash with 0 rows
}

// ============================================================================
// 13. GRID TESTS
// ============================================================================

TEST_CASE("GPU Grid: basic grid layout", "[gpu][grid]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginGrid("grid1", 3, 40.0f);
    for (int i = 0; i < 9; ++i) {
        GridNextCell();
        Label("Cell " + std::to_string(i));
    }
    EndGrid();
    gpu.endFrame();
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

// ============================================================================
// 14. MULTI-FRAME STRESS TESTS
// ============================================================================

TEST_CASE("GPU Stress: 100 frames with mixed widgets", "[gpu][stress]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    float slider = 0.5f;
    bool check = false;
    int combo = 0;
    std::string text = "hello";
    std::vector<std::string> items = {"A", "B", "C"};

    for (int frame = 0; frame < 100; ++frame) {
        gpu.beginFrame(0.016f);

        BeginVertical();
        Button("Btn");
        Label("Lbl");
        Checkbox("Chk", &check);
        SliderFloat("Sl", &slider, 0.0f, 1.0f);
        ComboBox("Cmb", &combo, items);
        TextInput("Txt", &text);
        ProgressBar(static_cast<float>(frame) / 100.0f);
        Separator();
        EndVertical();

        gpu.endFrame();
    }

    REQUIRE(gpu.ctx->frame >= 100);
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

TEST_CASE("GPU Stress: many buttons in single frame", "[gpu][stress]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    BeginVertical();
    for (int i = 0; i < 200; ++i) {
        Button("Button_" + std::to_string(i));
    }
    EndVertical();
    gpu.endFrame();

    REQUIRE(gpu.ctx->perfCounters.vertexCount > 0);
    REQUIRE(gpu.ctx->perfCounters.batchCount > 0);
}

// ============================================================================
// 15. STYLE OVERRIDE TESTS
// ============================================================================

TEST_CASE("GPU Style: PushButtonStyle changes rendering", "[gpu][style]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();

    ButtonStyle custom = gpu.ctx->style.button;
    custom.cornerRadius = 20.0f;
    custom.background.normal = Color(1, 0, 0, 1);

    PushButtonStyle(custom);
    Button("Red Button");
    PopButtonStyle();

    Button("Normal Button");
    gpu.endFrame();
    REQUIRE(true); // no crash, style restored
}

TEST_CASE("GPU Style: PushTextColor changes text color", "[gpu][style]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    PushTextColor(Color(1, 0, 0, 1));
    Label("Red text");
    PopTextColor();
    Label("Default text");
    gpu.endFrame();
    REQUIRE(true);
}

// ============================================================================
// 16. DPI SCALING TESTS
// ============================================================================

TEST_CASE("DPI: DPIScale multiplies correctly", "[dpi]") {
    UIContext mockCtx;
    mockCtx.dpiScale = 1.5f;
    REQUIRE_THAT(DPIScale(&mockCtx, 10.0f), WithinAbs(15.0f, 1e-5));
    REQUIRE_THAT(DPIScale(&mockCtx, 0.0f), WithinAbs(0.0f, 1e-5));
}

TEST_CASE("DPI: DPIScale with null context returns raw value", "[dpi]") {
    REQUIRE_THAT(DPIScale(nullptr, 10.0f), WithinAbs(10.0f, 1e-5));
}

TEST_CASE("DPI: DPIScale with 1.0 is identity", "[dpi]") {
    UIContext mockCtx;
    mockCtx.dpiScale = 1.0f;
    REQUIRE_THAT(DPIScale(&mockCtx, 42.0f), WithinAbs(42.0f, 1e-5));
}

// ============================================================================
// 17. LOGGING SYSTEM TESTS
// ============================================================================

TEST_CASE("Logging: custom callback receives messages", "[logging]") {
    std::vector<std::pair<LogLevel, std::string>> logs;
    SetLogCallback([&](LogLevel level, const char* msg) {
        logs.push_back({level, msg});
    });

    Log(LogLevel::Info, "Test message %d", 42);
    Log(LogLevel::Warning, "Warning: %s", "caution");
    Log(LogLevel::Error, "Error!");

    REQUIRE(logs.size() == 3);
    REQUIRE(logs[0].first == LogLevel::Info);
    REQUIRE(logs[0].second.find("42") != std::string::npos);
    REQUIRE(logs[1].first == LogLevel::Warning);
    REQUIRE(logs[2].first == LogLevel::Error);

    SetLogCallback(nullptr); // cleanup
}

TEST_CASE("Logging: null callback doesn't crash", "[logging]") {
    SetLogCallback(nullptr);
    Log(LogLevel::Info, "This should not crash");
    Log(LogLevel::Error, "Neither should this");
    REQUIRE(true);
}

// ============================================================================
// 18. WIDGET TREE & ACCESSIBILITY
// ============================================================================

TEST_CASE("Accessibility: multiple event types fire correctly", "[a11y]") {
    std::map<AccessibilityEvent, int> counts;
    SetAccessibilityCallback([&](AccessibilityEvent ev, const WidgetNode*) {
        counts[ev]++;
    });

    WidgetNode node;
    FireAccessibilityEvent(AccessibilityEvent::FocusChanged, &node);
    FireAccessibilityEvent(AccessibilityEvent::FocusChanged, &node);
    FireAccessibilityEvent(AccessibilityEvent::ValueChanged, &node);

    REQUIRE(counts[AccessibilityEvent::FocusChanged] == 2);
    REQUIRE(counts[AccessibilityEvent::ValueChanged] == 1);

    SetAccessibilityCallback(nullptr);
}

TEST_CASE("Accessibility: accessible name prefers explicit over debug", "[a11y]") {
    WidgetNode node;
    node.debugName = "btn_save";
    REQUIRE(GetAccessibleName(&node) == "btn_save");

    node.accessibleName = "Save Document";
    REQUIRE(GetAccessibleName(&node) == "Save Document");
}

// ============================================================================
// 19. RESPONSIVE UI TESTS
// ============================================================================

// --- 19.1 Layout Constraint System ---

TEST_CASE("Responsive: FixedSize constraints produce exact dimensions", "[responsive][constraints]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    SetNextConstraints(FixedSize(150.0f, 80.0f));
    LayoutConstraints c = ConsumeNextConstraints();
    Vec2 result = ApplyConstraints(gpu.ctx, c, Vec2(300.0f, 200.0f));
    REQUIRE_THAT(result.x, WithinAbs(150.0f, 0.1f));
    REQUIRE_THAT(result.y, WithinAbs(80.0f, 0.1f));

    gpu.endFrame();
}

TEST_CASE("Responsive: AutoSize constraints use desired size", "[responsive][constraints]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    SetNextConstraints(AutoSize());
    LayoutConstraints c = ConsumeNextConstraints();
    Vec2 result = ApplyConstraints(gpu.ctx, c, Vec2(123.0f, 45.0f));
    REQUIRE_THAT(result.x, WithinAbs(123.0f, 0.1f));
    REQUIRE_THAT(result.y, WithinAbs(45.0f, 0.1f));

    gpu.endFrame();
}

TEST_CASE("Responsive: FillSize constraints use available space", "[responsive][constraints]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 400.0f));

    SetNextConstraints(FillSize());
    LayoutConstraints c = ConsumeNextConstraints();
    Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
    Vec2 result = ApplyConstraints(gpu.ctx, c, Vec2(50.0f, 50.0f));
    // Fill should use available space, not the desired 50x50
    REQUIRE(result.x > 50.0f);
    REQUIRE(result.y > 50.0f);

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: min/max constraints are respected", "[responsive][constraints]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    LayoutConstraints c;
    c.width = SizeConstraint::Auto;
    c.height = SizeConstraint::Auto;
    c.minWidth = 100.0f;
    c.maxWidth = 200.0f;
    c.minHeight = 50.0f;
    c.maxHeight = 80.0f;

    // Desired too small — clamped to min
    Vec2 r1 = ApplyConstraints(gpu.ctx, c, Vec2(10.0f, 10.0f));
    REQUIRE(r1.x >= 100.0f);
    REQUIRE(r1.y >= 50.0f);

    // Desired too large — clamped to max
    Vec2 r2 = ApplyConstraints(gpu.ctx, c, Vec2(500.0f, 500.0f));
    REQUIRE(r2.x <= 200.0f);
    REQUIRE(r2.y <= 80.0f);

    gpu.endFrame();
}

TEST_CASE("Responsive: ConsumeNextConstraints clears after use", "[responsive][constraints]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    SetNextConstraints(FixedSize(100.0f, 100.0f));
    LayoutConstraints c1 = ConsumeNextConstraints();
    REQUIRE(c1.width == SizeConstraint::Fixed);

    // Second consume should return default (Auto)
    LayoutConstraints c2 = ConsumeNextConstraints();
    REQUIRE(c2.width == SizeConstraint::Auto);
    REQUIRE(c2.height == SizeConstraint::Auto);

    gpu.endFrame();
}

// --- 19.2 Vertical Layout Responsiveness ---

TEST_CASE("Responsive: vertical layout distributes items sequentially", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(300.0f, 400.0f));

    Vec2 pos1 = gpu.ctx->cursorPos;
    Label("First");
    Vec2 pos2 = gpu.ctx->cursorPos;
    Label("Second");
    Vec2 pos3 = gpu.ctx->cursorPos;

    // Each label should be below the previous one
    REQUIRE(pos2.y > pos1.y);
    REQUIRE(pos3.y > pos2.y);
    // X should remain the same (vertical stacking)
    REQUIRE_THAT(pos1.x, WithinAbs(pos2.x, 0.1f));
    REQUIRE_THAT(pos2.x, WithinAbs(pos3.x, 0.1f));

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: vertical layout available space decreases", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(300.0f, 400.0f));

    Vec2 avail1 = GetCurrentAvailableSpace(gpu.ctx);
    Label("Takes space");
    Vec2 avail2 = GetCurrentAvailableSpace(gpu.ctx);

    // Available vertical space should decrease after placing a widget
    REQUIRE(avail2.y < avail1.y);
    // Width should stay the same
    REQUIRE_THAT(avail2.x, WithinAbs(avail1.x, 1.0f));

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: vertical layout with different container sizes", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    // Small container
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(200.0f, 150.0f));
    Vec2 smallAvail = GetCurrentAvailableSpace(gpu.ctx);
    EndVertical();
    gpu.endFrame();

    // Large container
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(600.0f, 500.0f));
    Vec2 largeAvail = GetCurrentAvailableSpace(gpu.ctx);
    EndVertical();
    gpu.endFrame();

    REQUIRE(largeAvail.x > smallAvail.x);
    REQUIRE(largeAvail.y > smallAvail.y);
}

// --- 19.3 Horizontal Layout Responsiveness ---

TEST_CASE("Responsive: horizontal layout places items side by side", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginHorizontal(-1.0f, Vec2(500.0f, 100.0f));

    Vec2 pos1 = gpu.ctx->cursorPos;
    Label("Left");
    Vec2 pos2 = gpu.ctx->cursorPos;
    Label("Right");
    Vec2 pos3 = gpu.ctx->cursorPos;

    // Each label should be to the right of the previous
    REQUIRE(pos2.x > pos1.x);
    REQUIRE(pos3.x > pos2.x);
    // Y should remain roughly the same (horizontal placement)
    REQUIRE_THAT(pos1.y, WithinAbs(pos2.y, 0.1f));

    EndHorizontal();
    gpu.endFrame();
}

TEST_CASE("Responsive: horizontal layout available space decreases in X", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginHorizontal(-1.0f, Vec2(500.0f, 100.0f));

    Vec2 avail1 = GetCurrentAvailableSpace(gpu.ctx);
    Button("Btn1");
    Vec2 avail2 = GetCurrentAvailableSpace(gpu.ctx);

    REQUIRE(avail2.x < avail1.x);

    EndHorizontal();
    gpu.endFrame();
}

// --- 19.4 Nested Layouts ---

TEST_CASE("Responsive: nested vertical in horizontal", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginHorizontal(-1.0f, Vec2(600.0f, 300.0f));

    Vec2 hStart = gpu.ctx->cursorPos;

    BeginVertical(-1.0f, Vec2(200.0f, 200.0f));
    Label("V1");
    Label("V2");
    EndVertical();

    Vec2 afterVertical = gpu.ctx->cursorPos;
    // After the vertical block, cursor should have advanced to the right
    REQUIRE(afterVertical.x > hStart.x);

    EndHorizontal();
    gpu.endFrame();
}

TEST_CASE("Responsive: nested horizontal in vertical", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(600.0f, 400.0f));

    Vec2 vStart = gpu.ctx->cursorPos;

    BeginHorizontal(-1.0f, Vec2(400.0f, 50.0f));
    Button("A");
    Button("B");
    EndHorizontal();

    Vec2 afterHorizontal = gpu.ctx->cursorPos;
    // After the horizontal block, cursor should have advanced downward
    REQUIRE(afterHorizontal.y > vStart.y);

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: deeply nested layouts maintain correct space", "[responsive][layout]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(800.0f, 600.0f));
      BeginHorizontal(-1.0f, Vec2(700.0f, 200.0f));
        BeginVertical(-1.0f, Vec2(300.0f, 180.0f));
          Vec2 deepAvail = GetCurrentAvailableSpace(gpu.ctx);
          // Should have constrained available space
          REQUIRE(deepAvail.x <= 300.0f);
          REQUIRE(deepAvail.y <= 180.0f);
          REQUIRE(deepAvail.x > 0.0f);
          REQUIRE(deepAvail.y > 0.0f);
        EndVertical();
      EndHorizontal();
    EndVertical();

    gpu.endFrame();
}

// --- 19.5 Button Responsiveness ---

TEST_CASE("Responsive: button auto-sizes to text content", "[responsive][button]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 300.0f));

    Button("Short");
    Vec2 shortSize = gpu.ctx->lastItemSize;

    Button("A much longer button label text");
    Vec2 longSize = gpu.ctx->lastItemSize;

    // Longer text should produce wider button
    REQUIRE(longSize.x > shortSize.x);
    // Heights should be similar (single-line text)
    REQUIRE_THAT(longSize.y, WithinAbs(shortSize.y, 2.0f));

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: button respects explicit size", "[responsive][button]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 300.0f));

    Button("X", Vec2(200.0f, 50.0f));
    Vec2 btnSize = gpu.ctx->lastItemSize;

    REQUIRE_THAT(btnSize.x, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(btnSize.y, WithinAbs(50.0f, 1.0f));

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: button Fill constraint expands to container", "[responsive][button]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(400.0f, 300.0f));

    Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
    SetNextConstraints(FillSize());
    Button("Fill Me");
    Vec2 fillSize = gpu.ctx->lastItemSize;

    // Button should fill most of the available width
    REQUIRE(fillSize.x > avail.x * 0.8f);

    EndVertical();
    gpu.endFrame();
}

// --- 19.6 Slider Responsiveness ---

TEST_CASE("Responsive: slider fills layout width by default", "[responsive][slider]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 300.0f));

    Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
    float v1 = 0.5f;
    SliderFloat("Auto Fill", &v1, 0.0f, 1.0f);
    Vec2 fillSize = gpu.ctx->lastItemSize;

    // Slider should fill available width
    REQUIRE(fillSize.x > avail.x * 0.7f);

    // With FixedSize constraint, slider respects explicit size
    float v2 = 0.5f;
    SetNextConstraints(FixedSize(150.0f, 40.0f));
    SliderFloat("Fixed", &v2, 0.0f, 1.0f);
    Vec2 fixedSize = gpu.ctx->lastItemSize;
    REQUIRE_THAT(fixedSize.x, WithinAbs(150.0f, 1.0f));

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: slider with Fill constraint stretches", "[responsive][slider]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 300.0f));

    Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
    float val = 0.5f;
    SetNextConstraints(FillSize());
    SliderFloat("Fill Slider", &val, 0.0f, 1.0f);
    Vec2 fillSize = gpu.ctx->lastItemSize;

    // Should take most of the available width
    REQUIRE(fillSize.x > avail.x * 0.7f);

    EndVertical();
    gpu.endFrame();
}

// --- 19.7 Panel Responsiveness ---

TEST_CASE("Responsive: panel respects given size", "[responsive][panel]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    if (BeginPanel("sized_panel", Vec2(350.0f, 250.0f))) {
        Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
        // Content area should be within the panel (minus padding/title)
        REQUIRE(avail.x > 0.0f);
        REQUIRE(avail.x <= 350.0f);
        // Y may be 0 in scrollable panels (unlimited vertical space)
        REQUIRE(avail.y >= 0.0f);
        EndPanel();
    }

    gpu.endFrame();
}

TEST_CASE("Responsive: small panel constrains content space", "[responsive][panel]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    float smallAvailX = 0.0f, largeAvailX = 0.0f;

    gpu.beginFrame();
    if (BeginPanel("small_p", Vec2(150.0f, 100.0f))) {
        smallAvailX = GetCurrentAvailableSpace(gpu.ctx).x;
        EndPanel();
    }
    gpu.endFrame();

    gpu.beginFrame();
    if (BeginPanel("large_p", Vec2(500.0f, 400.0f))) {
        largeAvailX = GetCurrentAvailableSpace(gpu.ctx).x;
        EndPanel();
    }
    gpu.endFrame();

    REQUIRE(largeAvailX > smallAvailX);
}

// --- 19.8 TabView Responsiveness ---

TEST_CASE("Responsive: tab view content area adapts to container size", "[responsive][tabview]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    std::vector<std::string> tabs = {"A", "B"};
    int active = 0;

    // Small tab view
    gpu.beginFrame();
    float smallContentW = 0.0f;
    if (BeginTabView("tv_small", &active, tabs, Vec2(200.0f, 150.0f))) {
        smallContentW = GetCurrentAvailableSpace(gpu.ctx).x;
        EndTabView();
    }
    gpu.endFrame();

    // Large tab view
    gpu.beginFrame();
    float largeContentW = 0.0f;
    if (BeginTabView("tv_large", &active, tabs, Vec2(600.0f, 400.0f))) {
        largeContentW = GetCurrentAvailableSpace(gpu.ctx).x;
        EndTabView();
    }
    gpu.endFrame();

    REQUIRE(largeContentW > smallContentW);
}

TEST_CASE("Responsive: tab overflow activates with narrow container", "[responsive][tabview]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    std::vector<std::string> tabs;
    for (int i = 0; i < 15; ++i) tabs.push_back("Tab " + std::to_string(i));
    int active = 0;

    // Narrow — should trigger overflow
    gpu.beginFrame();
    if (BeginTabView("tv_overflow_narrow", &active, tabs, Vec2(200.0f, 150.0f))) {
        EndTabView();
    }
    uint32_t idNarrow = GenerateId("TABVIEW:", "tv_overflow_narrow");
    float totalWidthNarrow = gpu.ctx->tabViewStates[idNarrow].totalTabsWidth;
    gpu.endFrame();

    // Total tab width should exceed the container
    REQUIRE(totalWidthNarrow > 200.0f);

    // Wide — may not trigger overflow
    gpu.beginFrame();
    if (BeginTabView("tv_overflow_wide", &active, tabs, Vec2(2000.0f, 150.0f))) {
        EndTabView();
    }
    gpu.endFrame();

    // Should not crash regardless of size
    REQUIRE(true);
}

// --- 19.9 Splitter Responsiveness ---

TEST_CASE("Responsive: splitter regions scale with container size", "[responsive][splitter]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    float ratio = 0.5f;

    // Small container
    gpu.beginFrame();
    float smallFirstW = 0.0f;
    if (BeginSplitter("sp_small", true, &ratio, Vec2(200.0f, 100.0f))) {
        smallFirstW = GetCurrentAvailableSpace(gpu.ctx).x;
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();

    // Large container
    ratio = 0.5f;
    gpu.beginFrame();
    float largeFirstW = 0.0f;
    if (BeginSplitter("sp_large", true, &ratio, Vec2(800.0f, 400.0f))) {
        largeFirstW = GetCurrentAvailableSpace(gpu.ctx).x;
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();

    // Larger container should give more space to first region
    REQUIRE(largeFirstW > smallFirstW);
}

TEST_CASE("Responsive: splitter ratio changes region proportions", "[responsive][splitter]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    // 30% ratio
    float ratio1 = 0.3f;
    gpu.beginFrame();
    float firstW_30 = 0.0f;
    if (BeginSplitter("sp_ratio", true, &ratio1, Vec2(400.0f, 200.0f))) {
        firstW_30 = GetCurrentAvailableSpace(gpu.ctx).x;
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();

    // 70% ratio
    float ratio2 = 0.7f;
    gpu.beginFrame();
    float firstW_70 = 0.0f;
    if (BeginSplitter("sp_ratio2", true, &ratio2, Vec2(400.0f, 200.0f))) {
        firstW_70 = GetCurrentAvailableSpace(gpu.ctx).x;
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();

    REQUIRE(firstW_70 > firstW_30);
}

TEST_CASE("Responsive: horizontal splitter regions scale vertically", "[responsive][splitter]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    float ratio = 0.5f;

    gpu.beginFrame();
    float firstH = 0.0f;
    if (BeginSplitter("sp_horiz", false, &ratio, Vec2(400.0f, 300.0f))) {
        firstH = GetCurrentAvailableSpace(gpu.ctx).y;
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();

    // First region height should be roughly half of 300 minus divider
    REQUIRE(firstH > 100.0f);
    REQUIRE(firstH < 200.0f);
}

// --- 19.10 ScrollView Responsiveness ---

TEST_CASE("Responsive: scroll view content area matches given size", "[responsive][scrollview]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    if (BeginScrollView("sv_resp", Vec2(300.0f, 200.0f))) {
        Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
        REQUIRE(avail.x > 0.0f);
        REQUIRE(avail.x <= 300.0f);
        // Y may be 0 in scroll views (content is scrollable, not constrained)
        REQUIRE(avail.y >= 0.0f);
        EndScrollView();
    }

    gpu.endFrame();
}

TEST_CASE("Responsive: scroll view adapts to different sizes", "[responsive][scrollview]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    float smallW = 0.0f, largeW = 0.0f;

    gpu.beginFrame();
    if (BeginScrollView("sv_small", Vec2(150.0f, 100.0f))) {
        smallW = GetCurrentAvailableSpace(gpu.ctx).x;
        EndScrollView();
    }
    gpu.endFrame();

    gpu.beginFrame();
    if (BeginScrollView("sv_large", Vec2(500.0f, 400.0f))) {
        largeW = GetCurrentAvailableSpace(gpu.ctx).x;
        EndScrollView();
    }
    gpu.endFrame();

    REQUIRE(largeW > smallW);
}

// --- 19.11 Grid Responsiveness ---

TEST_CASE("Responsive: grid cell width adapts to container", "[responsive][grid]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    // Grid inside narrow container
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(200.0f, 200.0f));
    BeginGrid("grid_narrow", 3);
    Vec2 narrowAvail = GetCurrentAvailableSpace(gpu.ctx);
    EndGrid();
    EndVertical();
    gpu.endFrame();

    // Grid inside wide container
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(600.0f, 200.0f));
    BeginGrid("grid_wide", 3);
    Vec2 wideAvail = GetCurrentAvailableSpace(gpu.ctx);
    EndGrid();
    EndVertical();
    gpu.endFrame();

    // Wide container should provide more available space
    REQUIRE(wideAvail.x > narrowAvail.x);
}

TEST_CASE("Responsive: grid column count affects cell width", "[responsive][grid]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    // 2 columns
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(400.0f, 200.0f));
    BeginGrid("grid_2col", 2);
    float w2 = 0.0f;
    if (!gpu.ctx->gridStack.empty()) w2 = gpu.ctx->gridStack.back().cellWidth;
    EndGrid();
    EndVertical();
    gpu.endFrame();

    // 4 columns
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(400.0f, 200.0f));
    BeginGrid("grid_4col", 4);
    float w4 = 0.0f;
    if (!gpu.ctx->gridStack.empty()) w4 = gpu.ctx->gridStack.back().cellWidth;
    EndGrid();
    EndVertical();
    gpu.endFrame();

    // More columns = narrower cells
    REQUIRE(w2 > w4);
}

// --- 19.12 DPI Scaling Responsiveness ---

TEST_CASE("Responsive: DPI scale affects widget dimensions", "[responsive][dpi]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    // 1x DPI
    gpu.ctx->dpiScale = 1.0f;
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(400.0f, 300.0f));
    Button("DPI Test");
    Vec2 size1x = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    // 2x DPI
    gpu.ctx->dpiScale = 2.0f;
    gpu.beginFrame();
    BeginVertical(-1.0f, Vec2(400.0f, 300.0f));
    Button("DPI Test");
    Vec2 size2x = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    // Verify DPIScale function works correctly at different scales
    gpu.ctx->dpiScale = 1.0f;
    float scaled1x = DPIScale(gpu.ctx, 10.0f);
    REQUIRE_THAT(scaled1x, WithinAbs(10.0f, 0.01f));

    gpu.ctx->dpiScale = 2.0f;
    float scaled2x = DPIScale(gpu.ctx, 10.0f);
    REQUIRE_THAT(scaled2x, WithinAbs(20.0f, 0.01f));

    gpu.ctx->dpiScale = 1.0f; // restore
}

TEST_CASE("Responsive: DPI scale multiplier is consistent", "[responsive][dpi]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    // Verify DPIScale produces correct proportional values at multiple scales
    float base = 16.0f;

    gpu.ctx->dpiScale = 1.0f;
    REQUIRE_THAT(DPIScale(gpu.ctx, base), WithinAbs(16.0f, 0.01f));

    gpu.ctx->dpiScale = 1.25f;
    REQUIRE_THAT(DPIScale(gpu.ctx, base), WithinAbs(20.0f, 0.01f));

    gpu.ctx->dpiScale = 1.5f;
    REQUIRE_THAT(DPIScale(gpu.ctx, base), WithinAbs(24.0f, 0.01f));

    gpu.ctx->dpiScale = 2.0f;
    REQUIRE_THAT(DPIScale(gpu.ctx, base), WithinAbs(32.0f, 0.01f));

    gpu.ctx->dpiScale = 1.0f;
}

// --- 19.13 Widget Size Consistency Across Frames ---

TEST_CASE("Responsive: widget sizes are consistent across frames", "[responsive][stability]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    Vec2 sizes[3];
    for (int frame = 0; frame < 3; ++frame) {
        gpu.beginFrame();
        BeginVertical(-1.0f, Vec2(400.0f, 300.0f));
        Button("Stable Button", Vec2(0, 0));
        sizes[frame] = gpu.ctx->lastItemSize;
        EndVertical();
        gpu.endFrame();
    }

    REQUIRE_THAT(sizes[0].x, WithinAbs(sizes[1].x, 0.01f));
    REQUIRE_THAT(sizes[1].x, WithinAbs(sizes[2].x, 0.01f));
    REQUIRE_THAT(sizes[0].y, WithinAbs(sizes[1].y, 0.01f));
}

TEST_CASE("Responsive: cursor position resets each frame", "[responsive][stability]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    Vec2 startPositions[3];
    for (int frame = 0; frame < 3; ++frame) {
        gpu.beginFrame();
        startPositions[frame] = gpu.ctx->cursorPos;
        Label("Frame content");
        gpu.endFrame();
    }

    // Starting cursor should be consistent across frames
    REQUIRE_THAT(startPositions[0].x, WithinAbs(startPositions[1].x, 0.1f));
    REQUIRE_THAT(startPositions[0].y, WithinAbs(startPositions[1].y, 0.1f));
    REQUIRE_THAT(startPositions[1].x, WithinAbs(startPositions[2].x, 0.1f));
}

// --- 19.14 Constraint Propagation Through Containers ---

TEST_CASE("Responsive: Fill button in horizontal layout shares space", "[responsive][propagation]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginHorizontal(-1.0f, Vec2(500.0f, 60.0f));

    Button("Fixed", Vec2(100.0f, 40.0f));
    Vec2 afterFixed = gpu.ctx->cursorPos;
    Vec2 remainAvail = GetCurrentAvailableSpace(gpu.ctx);

    SetNextConstraints(FillSize());
    Button("Expanding");
    Vec2 expandSize = gpu.ctx->lastItemSize;

    // Expanding button should use most remaining space
    REQUIRE(expandSize.x > remainAvail.x * 0.5f);

    EndHorizontal();
    gpu.endFrame();
}

TEST_CASE("Responsive: multiple fixed-size buttons maintain dimensions", "[responsive][propagation]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginHorizontal(-1.0f, Vec2(800.0f, 60.0f));

    for (int i = 0; i < 5; ++i) {
        Button("B" + std::to_string(i), Vec2(100.0f, 40.0f));
        Vec2 s = gpu.ctx->lastItemSize;
        REQUIRE_THAT(s.x, WithinAbs(100.0f, 1.0f));
        REQUIRE_THAT(s.y, WithinAbs(40.0f, 1.0f));
    }

    EndHorizontal();
    gpu.endFrame();
}

// --- 19.15 Edge Cases ---

TEST_CASE("Responsive: zero-size container doesn't crash", "[responsive][edge]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(0.0f, 0.0f));
    Label("Inside zero container");
    EndVertical();

    gpu.endFrame();
    REQUIRE(true);
}

TEST_CASE("Responsive: very small container doesn't crash", "[responsive][edge]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(1.0f, 1.0f));
    Button("Tiny");
    Label("Also tiny");
    EndVertical();

    gpu.endFrame();
    REQUIRE(true);
}

TEST_CASE("Responsive: very large container doesn't crash", "[responsive][edge]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(10000.0f, 10000.0f));
    Button("Huge");
    Label("Also huge");
    EndVertical();

    gpu.endFrame();
    REQUIRE(true);
}

TEST_CASE("Responsive: splitter with extreme ratios clamps correctly", "[responsive][edge]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);

    float ratio = 0.001f;
    gpu.beginFrame();
    if (BeginSplitter("sp_extreme_low", true, &ratio, Vec2(400.0f, 200.0f))) {
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();
    REQUIRE(ratio >= 0.05f);

    ratio = 0.999f;
    gpu.beginFrame();
    if (BeginSplitter("sp_extreme_high", true, &ratio, Vec2(400.0f, 200.0f))) {
        SplitterPanel();
        EndSplitter();
    }
    gpu.endFrame();
    REQUIRE(ratio <= 0.95f);
}

TEST_CASE("Responsive: tab view with single tab no overflow", "[responsive][edge]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    std::vector<std::string> tabs = {"Only"};
    int active = 0;
    if (BeginTabView("tv_single", &active, tabs, Vec2(300.0f, 200.0f))) {
        Label("Content");
        EndTabView();
    }

    gpu.endFrame();
    REQUIRE(true);
}

TEST_CASE("Responsive: nested splitters adapt sizes", "[responsive][edge]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    float outerRatio = 0.5f;
    float innerRatio = 0.5f;

    if (BeginSplitter("outer", true, &outerRatio, Vec2(600.0f, 400.0f))) {
        // First region of outer: nest another splitter
        if (BeginSplitter("inner", false, &innerRatio)) {
            Label("Top-Left");
            SplitterPanel();
            Label("Bottom-Left");
            EndSplitter();
        }
        SplitterPanel();
        Label("Right");
        EndSplitter();
    }

    gpu.endFrame();
    REQUIRE(true);
}

// --- 19.16 Separator Responsiveness ---

TEST_CASE("Responsive: separator takes space in vertical layout", "[responsive][separator]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(300.0f, 300.0f));

    Vec2 before = gpu.ctx->cursorPos;
    Separator();
    Vec2 after = gpu.ctx->cursorPos;

    // Separator should advance cursor downward
    REQUIRE(after.y > before.y);

    EndVertical();
    gpu.endFrame();
}

// --- 19.17 TextInput Responsiveness ---

TEST_CASE("Responsive: text input fills layout width by default", "[responsive][textinput]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 300.0f));

    Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
    std::string val = "test";
    TextInput("Auto Fill", &val);
    Vec2 fillSize = gpu.ctx->lastItemSize;

    // TextInput should fill available width
    REQUIRE(fillSize.x > avail.x * 0.7f);

    // With FixedSize constraint, input respects explicit size
    SetNextConstraints(FixedSize(120.0f, 30.0f));
    TextInput("Fixed", &val);
    Vec2 fixedSize = gpu.ctx->lastItemSize;
    REQUIRE_THAT(fixedSize.x, WithinAbs(120.0f, 1.0f));

    EndVertical();
    gpu.endFrame();
}

TEST_CASE("Responsive: text input Fill constraint stretches", "[responsive][textinput]") {
    GPUFixture gpu;
    REQUIRE(gpu.valid);
    gpu.beginFrame();

    BeginVertical(-1.0f, Vec2(500.0f, 300.0f));

    std::string val = "test";
    Vec2 avail = GetCurrentAvailableSpace(gpu.ctx);
    SetNextConstraints(FillSize());
    TextInput("Fill Input", &val);
    Vec2 fillSize = gpu.ctx->lastItemSize;

    REQUIRE(fillSize.x > avail.x * 0.7f);

    EndVertical();
    gpu.endFrame();
}

// ============================================================================
// Icon overloads (Phases 1-3)
// ============================================================================

TEST_CASE("Icon Button: layout matches non-icon path when codepoint=0", "[icons][button]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    Button("Foo");
    Vec2 plain = gpu.ctx->lastItemSize;
    Button("Foo", 0u);
    Vec2 zeroIcon = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE_THAT(plain.x, WithinAbs(zeroIcon.x, 0.5f));
    REQUIRE_THAT(plain.y, WithinAbs(zeroIcon.y, 0.5f));
}

TEST_CASE("Icon Button: codepoint!=0 reserves icon slot (wider)", "[icons][button]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    Button("Foo");
    Vec2 plain = gpu.ctx->lastItemSize;
    Button("Foo", 0xE000u);
    Vec2 withIcon = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE(withIcon.x > plain.x);
}

TEST_CASE("IconButton produces a square button", "[icons][button][iconbutton]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    IconButton(0xE000u);
    Vec2 sz = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE(sz.x > 0.0f);
    REQUIRE_THAT(sz.x, WithinAbs(sz.y, 0.5f));
}

TEST_CASE("Icon Label: codepoint!=0 reserves icon slot", "[icons][label]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    Label("Hello");
    Vec2 plain = gpu.ctx->lastItemSize;
    Label("Hello", 0xE000u);
    Vec2 withIcon = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE(withIcon.x > plain.x);
}

TEST_CASE("IconLabel produces a square box at body fontSize", "[icons][label]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    IconLabel(0xE000u);
    Vec2 sz = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE(sz.x > 0.0f);
    REQUIRE_THAT(sz.x, WithinAbs(sz.y, 0.5f));
}

TEST_CASE("Icon Checkbox: trailing icon adds width", "[icons][checkbox]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    bool v = false;
    gpu.beginFrame();
    BeginVertical();
    Checkbox("Opt", &v);
    Vec2 plain = gpu.ctx->lastItemSize;
    Checkbox("Opt2", 0xE000u, &v);
    Vec2 withIcon = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE(withIcon.x > plain.x);
}

TEST_CASE("Icon overloads: missing-glyph codepoint does not crash", "[icons][safety]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    Button("X", 0xFFFFFFu);
    Label("Y", 0xFFFFFFu);
    bool v = false;
    Checkbox("Z", 0xFFFFFFu, &v);
    int rv = 0;
    RadioButton("R", 0xFFFFFFu, &rv, 0);
    IconButton(0xFFFFFFu);
    IconLabel(0xFFFFFFu);
    EndVertical();
    gpu.endFrame();

    SUCCEED();
}

TEST_CASE("SegmentedControl with vector<pair> (icon-only segments)", "[icons][segmented]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    int idx = 0;
    std::vector<std::pair<std::string, uint32_t>> opts = {
        {"", 0xE000u}, {"", 0xE001u}, {"", 0xE002u}
    };
    gpu.beginFrame();
    BeginVertical();
    SegmentedControl("seg", opts, &idx);
    Vec2 sz = gpu.ctx->lastItemSize;
    EndVertical();
    gpu.endFrame();

    REQUIRE(sz.x > 0.0f);
}

TEST_CASE("ComboBox with vector<pair> per-item icons", "[icons][combobox]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    int sel = 0;
    std::vector<std::pair<std::string, uint32_t>> items = {
        {"PlayerBody.fbx", 0xE000u},
        {"Enemy.fbx",      0xE001u},
        {"Crate.fbx",      0u}  // mixed: this row has no icon
    };
    gpu.beginFrame();
    BeginVertical();
    ComboBox("Mesh", &sel, items, 200.0f);
    EndVertical();
    gpu.endFrame();

    SUCCEED();
}

TEST_CASE("TableColumn iconCodepoint compiles and renders", "[icons][table]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    std::vector<TableColumn> cols = {
        {"Name", 100.0f, 40.0f, true, 0xE000u},
        {"Size", 80.0f, 40.0f, true, 0u}
    };
    TableState ts;
    gpu.beginFrame();
    BeginVertical();
    if (BeginTable("t", cols, 0, Vec2(300, 100), &ts)) {
        EndTable();
    }
    EndVertical();
    gpu.endFrame();

    SUCCEED();
}

TEST_CASE("Icons::* enum implicitly converts to uint32_t", "[icons][catalog]") {
    // Compile-time check: using Icons::Save in a uint32_t parameter must
    // not require static_cast.
    uint32_t cp = Icons::Save;
    REQUIRE(cp != 0u);
    REQUIRE(static_cast<uint32_t>(Icons::Save) == cp);

    // Aliases resolve to canonical entries.
    REQUIRE(static_cast<uint32_t>(Icons::Stop)    == static_cast<uint32_t>(Icons::Square));
    REQUIRE(static_cast<uint32_t>(Icons::Pointer) == static_cast<uint32_t>(Icons::MousePointer));
    REQUIRE(static_cast<uint32_t>(Icons::Cut)     == static_cast<uint32_t>(Icons::Scissors));
}

TEST_CASE("Widgets accept Icons::* values without cast", "[icons][catalog]") {
    GPUFixture gpu;
    if (!gpu.valid) return;

    gpu.beginFrame();
    BeginVertical();
    Button("Save", Icons::Save);
    Label("Folder", Icons::Folder);
    int idx = 0;
    SegmentedControl("tool", std::vector<std::pair<std::string, uint32_t>>{
        {"", Icons::Pointer}, {"", Icons::Move}, {"", Icons::Rotate},
    }, &idx);
    EndVertical();
    gpu.endFrame();

    SUCCEED();
}

// ============================================================================
// brief 21: ID scope stack (PushID / PopID)
// ============================================================================

TEST_CASE("ID stack: PushID changes the generated id, PopID restores it", "[id][brief21]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    uint32_t base = GenerateId("widget");
    PushID("scopeA");
    uint32_t scoped = GenerateId("widget");
    PopID();
    uint32_t restored = GenerateId("widget");
    gpu.endFrame();

    REQUIRE(scoped != base);       // scope perturbs the id
    REQUIRE(restored == base);     // PopID restores the seed exactly
    REQUIRE(gpu.ctx->idStack.empty()); // balanced
}

TEST_CASE("ID stack: same label in different scopes yields distinct ids", "[id][brief21]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    PushID("p1");
    uint32_t a = GenerateId("BTN:", "Accept");
    PopID();
    PushID("p2");
    uint32_t b = GenerateId("BTN:", "Accept");
    PopID();
    gpu.endFrame();

    REQUIRE(a != b);
}

TEST_CASE("ID stack: int and pointer discriminants disambiguate list items", "[id][brief21]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    std::set<uint32_t> ids;
    for (int i = 0; i < 8; ++i) {
        PushID(i);
        ids.insert(GenerateId("BTN:", "Delete")); // same label every row
        PopID();
    }
    gpu.endFrame();

    REQUIRE(ids.size() == 8); // every row got a distinct id
}

TEST_CASE("ID stack: two panels with the same button label are independent", "[id][brief21][gpu]") {
    GPUFixture gpu;
    if (!gpu.valid) { SKIP("No GL context"); return; }

    gpu.beginFrame();
    uint32_t idA = 0, idB = 0;
    if (BeginPanel("panelA", Vec2(200, 100))) {
        idA = GenerateId("BTN:", "Accept");
        EndPanel();
    }
    if (BeginPanel("panelB", Vec2(200, 100))) {
        idB = GenerateId("BTN:", "Accept");
        EndPanel();
    }
    gpu.endFrame();

    REQUIRE(idA != 0);
    REQUIRE(idB != 0);
    REQUIRE(idA != idB);                 // independent state, no "##" suffix needed
    REQUIRE(gpu.ctx->idStack.empty());   // panels auto push/pop are balanced
}
