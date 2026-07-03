#include <catch2/catch_test_macros.hpp>
#include "core/NullPlatform.h"
#include "core/InputState.h"

using namespace FluentUI;

// brief 25 acceptance: the UI must be drivable without SDL. NullPlatform lets a
// host inject synthetic UIEvents; PollEvent hands them back, and feeding them to a
// neutral InputState makes the input state react — exactly the path a widget sees.
// This proves the platform seam is real (no SDL required to conduct the core).

TEST_CASE("NullPlatform delivers injected events via PollEvent (FIFO)", "[platform][null]") {
    NullPlatform platform;

    UIEvent move{};
    move.type = UIEventType::MouseMove;
    move.x = 42.0f; move.y = 17.0f;
    platform.PushEvent(move);

    UIEvent down{};
    down.type = UIEventType::MouseButton;
    down.button = 0;
    down.pressed = true;
    down.x = 42.0f; down.y = 17.0f;
    platform.PushEvent(down);

    UIEvent out{};
    REQUIRE(platform.PollEvent(out));
    REQUIRE(out.type == UIEventType::MouseMove);
    REQUIRE(out.x == 42.0f);
    REQUIRE(out.y == 17.0f);

    REQUIRE(platform.PollEvent(out));
    REQUIRE(out.type == UIEventType::MouseButton);
    REQUIRE(out.pressed);

    // Queue drained → false.
    REQUIRE_FALSE(platform.PollEvent(out));
}

TEST_CASE("NullPlatform drives InputState end-to-end (mouse click over a rect)", "[platform][null]") {
    NullPlatform platform;
    InputState input;

    // A synthetic "button" rect [40,10]..[140,42].
    auto pointInButton = [](float x, float y) {
        return x >= 40.0f && x <= 140.0f && y >= 10.0f && y <= 42.0f;
    };

    // Host injects: move onto the button, then press, then release — no SDL.
    UIEvent e{};
    e.type = UIEventType::MouseMove; e.x = 80.0f; e.y = 24.0f;
    platform.PushEvent(e);
    e = UIEvent{}; e.type = UIEventType::MouseButton; e.button = 0; e.pressed = true; e.x = 80.0f; e.y = 24.0f;
    platform.PushEvent(e);

    // Frame 1: pump the queue into the input state (the driver's inner loop).
    input.Update();
    UIEvent ui{};
    while (platform.PollEvent(ui)) input.ProcessEvent(ui);

    REQUIRE(input.MouseX() == 80.0f);
    REQUIRE(input.MouseY() == 24.0f);
    REQUIRE(pointInButton(input.MouseX(), input.MouseY()));
    REQUIRE(input.IsMouseDown(0));
    REQUIRE(input.IsMousePressed(0)); // "pressed this frame" — a button would fire on release,
                                      // but the press edge is what the widget layer keys on.

    // Frame 2: release. IsMousePressed is a per-frame edge, so it must clear;
    // IsMouseReleased must now be set.
    e = UIEvent{}; e.type = UIEventType::MouseButton; e.button = 0; e.pressed = false; e.x = 80.0f; e.y = 24.0f;
    platform.PushEvent(e);

    input.Update(); // clears per-frame edges (pressed/released)
    REQUIRE_FALSE(input.IsMousePressed(0));
    while (platform.PollEvent(ui)) input.ProcessEvent(ui);

    REQUIRE_FALSE(input.IsMouseDown(0));
    REQUIRE(input.IsMouseReleased(0));
}

TEST_CASE("NullPlatform stubs are usable (clipboard + ticks + handle)", "[platform][null]") {
    NullPlatform platform;

    // Clipboard round-trips through the internal buffer (no OS clipboard).
    platform.SetClipboardText("hello");
    REQUIRE(platform.GetClipboardText() == "hello");

    // A dummy window handle is non-null and the OS calls are safe no-ops.
    WindowHandle w = platform.CreateWindowHandle("t", 320, 240, UIWindow_Resizable);
    REQUIRE(w != nullptr);
    int fw = 0, fh = 0;
    platform.SetFramebufferSize(320, 240);
    platform.GetFramebufferSize(w, fw, fh);
    REQUIRE(fw == 320);
    REQUIRE(fh == 240);
    REQUIRE(platform.GetDpiScale(w) == 1.0f);
    platform.SetWindowTitle(w, "t2");
    platform.Present(w);
    platform.DestroyWindowHandle(w);

    // GetTicksMs is monotonic non-decreasing.
    uint64_t t0 = platform.GetTicksMs();
    uint64_t t1 = platform.GetTicksMs();
    REQUIRE(t1 >= t0);
}
