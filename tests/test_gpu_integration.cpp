#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Accessibility.h"
#include "UI/Widgets.h"
#include "Math/Rect.h"

// NOTE: These tests require a real SDL+GL context.
// On CI, run with Mesa/llvmpipe (headless software rendering).
// Tests are skipped gracefully if SDL_Init or GL context creation fails.

using namespace FluentUI;

static bool g_sdlInitialized = false;
static SDL_Window* g_window = nullptr;

static bool EnsureGLContext() {
    if (g_window) return true;

    if (!g_sdlInitialized) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            WARN("SDL_Init failed, skipping GPU tests: " << SDL_GetError());
            return false;
        }
        g_sdlInitialized = true;
    }

    // Try to create a hidden window for offscreen rendering
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    g_window = SDL_CreateWindow("FluentUI GPU Tests", 800, 600,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!g_window) {
        WARN("Failed to create GL window: " << SDL_GetError());
        return false;
    }
    return true;
}

// ============================================================================
// Renderer Batching Tests
// ============================================================================

TEST_CASE("GPU: Context creation and destruction", "[gpu]") {
    if (!EnsureGLContext()) { SKIP("No GL context"); return; }

    UIContext* ctx = CreateContext(g_window);
    REQUIRE(ctx != nullptr);
    REQUIRE(ctx->initialized);

    DestroyContext();
}

TEST_CASE("GPU: Renderer draws without crash", "[gpu]") {
    if (!EnsureGLContext()) { SKIP("No GL context"); return; }

    UIContext* ctx = CreateContext(g_window);
    REQUIRE(ctx != nullptr);

    // Simulate one frame
    NewFrame(0.016f);

    ctx->renderer.DrawRectFilled(Vec2(10, 10), Vec2(100, 50), Color(1, 0, 0, 1), 4.0f);
    ctx->renderer.DrawRect(Vec2(10, 10), Vec2(100, 50), Color(1, 1, 1, 1), 4.0f);
    ctx->renderer.DrawText(Vec2(20, 20), "Test", Color(1, 1, 1, 1), 14.0f);
    ctx->renderer.DrawCircle(Vec2(50, 50), 20.0f, Color(0, 1, 0, 1), true);
    ctx->renderer.DrawLine(Vec2(0, 0), Vec2(100, 100), Color(1, 1, 0, 1), 2.0f);

    REQUIRE(ctx->perfCounters.batchCount == 0); // Not flushed yet

    RenderDeferredDropdowns();
    Render();

    REQUIRE(ctx->perfCounters.batchCount > 0);
    REQUIRE(ctx->perfCounters.vertexCount > 0);

    DestroyContext();
}

TEST_CASE("GPU: Multi-frame cycle", "[gpu]") {
    if (!EnsureGLContext()) { SKIP("No GL context"); return; }

    UIContext* ctx = CreateContext(g_window);
    REQUIRE(ctx != nullptr);

    // Run 10 frames
    for (int i = 0; i < 10; ++i) {
        NewFrame(0.016f);
        ctx->renderer.DrawRectFilled(Vec2(0, 0), Vec2(50, 50), Color(1, 1, 1, 1));
        RenderDeferredDropdowns();
        Render();
    }

    REQUIRE(ctx->frame >= 10);
    DestroyContext();
}

TEST_CASE("GPU: Font loading", "[gpu]") {
    if (!EnsureGLContext()) { SKIP("No GL context"); return; }

    UIContext* ctx = CreateContext(g_window);
    REQUIRE(ctx != nullptr);

    if (!ctx->renderer.IsFontLoaded()) {
        WARN("Font not loaded (assets/ not found) — skipping font tests");
        DestroyContext();
        SKIP("No font assets");
        return;
    }

    // Measure text should return non-zero size
    Vec2 size = ctx->renderer.MeasureText("Hello", 16.0f);
    REQUIRE(size.x > 0.0f);
    REQUIRE(size.y > 0.0f);

    DestroyContext();
}

// ============================================================================
// Clip and Scissor Tests
// ============================================================================

TEST_CASE("GPU: Clip rect push/pop", "[gpu]") {
    if (!EnsureGLContext()) { SKIP("No GL context"); return; }

    UIContext* ctx = CreateContext(g_window);
    REQUIRE(ctx != nullptr);

    NewFrame(0.016f);

    ctx->renderer.PushClipRect(Vec2(10, 10), Vec2(100, 100));
    ctx->renderer.DrawRectFilled(Vec2(20, 20), Vec2(50, 50), Color(1, 0, 0, 1));
    ctx->renderer.PopClipRect();

    RenderDeferredDropdowns();
    Render();

    REQUIRE(ctx->perfCounters.clipPushes > 0);

    DestroyContext();
}

// ============================================================================
// Accessibility Integration
// ============================================================================

TEST_CASE("Accessibility: role to string", "[a11y]") {
    REQUIRE(std::string(AccessibleRoleToString(WidgetNode::AccessibleRole::Button)) == "Button");
    REQUIRE(std::string(AccessibleRoleToString(WidgetNode::AccessibleRole::None)) == "None");
    REQUIRE(std::string(AccessibleRoleToString(WidgetNode::AccessibleRole::Slider)) == "Slider");
}

TEST_CASE("Accessibility: GetAccessibleName fallback", "[a11y]") {
    WidgetNode node;
    node.debugName = "my_button";
    REQUIRE(GetAccessibleName(&node) == "my_button");

    node.accessibleName = "Click Me";
    REQUIRE(GetAccessibleName(&node) == "Click Me");
}

TEST_CASE("Accessibility: event callback fires", "[a11y]") {
    int eventCount = 0;
    SetAccessibilityCallback([&](AccessibilityEvent, const WidgetNode*) {
        eventCount++;
    });

    WidgetNode node;
    FireAccessibilityEvent(AccessibilityEvent::FocusChanged, &node);
    FireAccessibilityEvent(AccessibilityEvent::ValueChanged, &node);
    REQUIRE(eventCount == 2);

    // Cleanup
    SetAccessibilityCallback(nullptr);
}
